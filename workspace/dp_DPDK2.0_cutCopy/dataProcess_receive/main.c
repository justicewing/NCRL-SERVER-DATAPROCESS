//版本号:1.1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <semaphore.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include "mkl.h"
#include "thread_pool.h"
#include "srslte/fec/cbsegm.h"
#include "TaskScheduler.h"
#include "config.h"

const int threadNum_tx = 1; // 发送端线程数
const int threadNum_rx = 1; // 接收端线程数

/* 声明互斥锁 */
pthread_mutex_t mutex1_tx;
pthread_mutex_t mutex2_tx;
pthread_mutex_t mutex1_rx;
pthread_mutex_t mutex2_rx;
pthread_mutex_t mutex3_rx;

/* 声明信号量 */
sem_t tx_can_be_destroyed;
sem_t rx_can_be_destroyed;
sem_t tx_buff_can_be_destroyed;
sem_t rx_buff_can_be_destroyed;
sem_t tx_prepared;
sem_t rx_prepared;
sem_t tx_buff_prepared;
sem_t rx_buff_prepared;
sem_t cache_tx;
sem_t cache_rx;
sem_t sem_buff_full;
sem_t sem_buff_empty;

/* 声明信号量 */
sem_t lcore_send_prepared;
sem_t lcore_receive_prepared;
sem_t lcore_p_prepared;
sem_t lcore_c_prepared;
// sem_t feedback_sem;

extern uint8_t *databuff;
extern pthread_mutex_t mutex_buff_empty;
extern int buff_empty;
extern int index_rx_write;
extern struct package_t package_rx[PACK_CACHE];
extern pthread_mutex_t mutex_readyNum_rx;
extern pthread_mutex_t mutex_startNum_rx;
extern int readyNum_rx;
extern int startNum_rx;

int feedbackable;

#define SENDABLE_FLAG 0xFF
#define SEND_TOKEN_INIT 0

int sendable;
int send_token = SEND_TOKEN_INIT;
pthread_mutex_t mutex_send_token;

volatile bool force_quit;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF 8192
#define MBUFF_DATA_LENGTH 1200
#define SEG_SIZE 1792

uint8_t *data_to_be_sent;

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 4 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 2048
#define RTE_TEST_TX_DESC_DEFAULT 2048
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf
{
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split = 0,   /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame = 0,	/**< Jumbo Frame Support disabled */
		.hw_strip_crc = 1,   /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *l2fwd_pktmbuf_pool = NULL;
struct rte_ring *ring_send;
struct rte_ring *ring_receive;
/* Per-port statistics struct */
struct l2fwd_port_statistics
{
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 1; /* default period is 10 seconds */

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = {27, '[', '2', 'J', '\0'};
	const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");
	portid = 0;
	/* skip disabled ports */
	printf("\nStatistics for port %u ------------------------------"
		   "\nPackets sent: %24" PRIu64
		   "\nPackets received: %20" PRIu64
		   "\nPackets dropped: %21" PRIu64,
		   portid,
		   port_statistics[portid].tx,
		   port_statistics[portid].rx,
		   port_statistics[portid].dropped);

	total_packets_dropped += port_statistics[portid].dropped;
	total_packets_tx += port_statistics[portid].tx;
	total_packets_rx += port_statistics[portid].rx;
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18" PRIu64
		   "\nTotal packets received: %14" PRIu64
		   "\nTotal packets dropped: %15" PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\n====================================================\n");
}
static void
print_mbuf_send(struct rte_mbuf *m)
{
	uint8_t *adcnt;
	FILE *fp;
	fp = fopen("send_data.txt", "a");
	fprintf(fp, "buf_addr:%d\n", m->buf_addr);
	fprintf(fp, "pkt_len:%d\n", m->pkt_len);
	fprintf(fp, "data_len:%d\n", m->data_len);
	fprintf(fp, "buf_len:%d\n", m->buf_len);
	fprintf(fp, "rte_pktmbuf_mtod(m):%d\n", rte_pktmbuf_mtod(m, void *));
	for (adcnt = (uint8_t *)m->buf_addr; adcnt < (uint8_t *)m->buf_addr + m->buf_len; adcnt++)
		fprintf(fp, "%02X ", *adcnt);
	fprintf(fp, "over\n");
	fclose(fp);
}
static void
print_mbuf_receive(struct rte_mbuf *m)
{
	uint8_t *adcnt;
	FILE *fp;
	fp = fopen("receive_data.txt", "a");
	fprintf(fp, "buf_addr:%d\n", m->buf_addr);
	fprintf(fp, "pkt_len:%d\n", m->pkt_len);
	fprintf(fp, "data_len:%d\n", m->data_len);
	fprintf(fp, "buf_len:%d\n", m->buf_len);
	fprintf(fp, "rte_pktmbuf_mtod(m):%d\n", rte_pktmbuf_mtod(m, void *));
	for (adcnt = (uint8_t *)m->buf_addr; adcnt < (uint8_t *)m->buf_addr + m->buf_len; adcnt++)
		fprintf(fp, "%02X ", *adcnt);
	fprintf(fp, "over\n");
	fclose(fp);
}

int package(uint8_t *data, int length, struct rte_mbuf *m)
{
	uint8_t *adcnt = NULL;
	int cnt = 0;

	rte_pktmbuf_append(m, length + 16);
	adcnt = (uint8_t *)rte_pktmbuf_mtod(m, uint8_t *);
	for (int i = 0; i < 16; i++)
	{
		*adcnt = 0xFF;
		adcnt++;
	}

	for (cnt = 0; cnt < length; cnt++)
	{
		*adcnt = data[cnt];
		adcnt++;
	}

	return 0;
}

/* 直接卸载到Rx缓冲区 */
int depackage(struct package_t *pkg_rx, uint8_t *adcnt)
{
	adcnt += 16;

	int8_t type = *adcnt;
	adcnt++;
	int16_t num = *adcnt << 8 + *(adcnt + 1);
	adcnt += 2;
	int16_t length = *adcnt << 8 + *(adcnt + 1);
	adcnt += 2;

	if (type == 0)
	{
		uint8_t *tbs_p = (uint8_t *)(pkg_rx->tbs);
		for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
		{
			*tbs_p = *adcnt;
			adcnt++;
			tbs_p++;
		}

		uint8_t *cqi_p = (uint8_t *)(pkg_rx->CQI_index);
		for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
		{
			*cqi_p = *adcnt;
			adcnt++;
			cqi_p++;
		}

		uint8_t *snr_p = (uint8_t *)(&(pkg_rx->SNR));
		for (int i = 0; i < sizeof(float); i++)
		{
			*snr_p = *adcnt;
			adcnt++;
			snr_p++;
		}
	}
	else if (type == 1)
	{
		int8_t *data = (int8_t *)pkg_rx->y + length * num;
		for (int cnt = 0; cnt < length; cnt++)
		{
			data[cnt] = *adcnt;
			adcnt++;
		}
	}
	else if (type >= 2 && type < 2 + MAX_BEAM)
	{
		int num_data = type - 2;
		int8_t *data = (int8_t *)pkg_rx->data[num_data] + length * num;
		for (int cnt = 0; cnt < length; cnt++)
		{
			data[cnt] = *adcnt;
			adcnt++;
		}
	}
	if (type == 1 + MAX_BEAM && num == MAX_CQI_MOD * DATA_SYM_NUM - 1)
	{
		index_rx_write++;
		if (index_rx_write >= PACK_CACHE)
			index_rx_write = 0;
		pthread_mutex_lock(&mutex_readyNum_rx);
		readyNum_rx++;
		pthread_mutex_unlock(&mutex_readyNum_rx);
		sem_post(&cache_rx);
	}

	return 0;
}

static void
l2fwd_main_loop_send(void)
{

	void *d = NULL;
	void **e = &d;
	struct rte_mbuf *m;
	int sent;
	long long int running_second = 1;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
							   BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id]; //此处每一个lcore从全局的conf中获取属于自己的conf

	double receive_rate = 0;
	double send_rate = 0;
	int mac_length_send = MBUFF_DATA_LENGTH;
	int mac_length_receive = MBUFF_DATA_LENGTH;

	RTE_LOG(INFO, L2FWD, "entering main loop send on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++)
	{

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
				portid);
	}

	sem_post(&lcore_send_prepared);
	sem_wait(&lcore_p_prepared);

	while (!force_quit)
	{

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely((diff_tsc > drain_tsc)) && send_token > 0)
		{
			portid = 0;
			buffer = tx_buffer[portid];

			// for (j = 0; j < 4; j++)
			// {
			if (rte_ring_mc_dequeue(ring_send, e) < 0)
				;
			else
			{
				pthread_mutex_lock(&mutex_send_token);
				send_token--;
				pthread_mutex_unlock(&mutex_send_token);
				// print_mbuf_send(*(struct rte_mbuf **)e);
				sent = rte_eth_tx_buffer(portid, 0, buffer, *(struct rte_mbuf **)e);
				port_statistics[portid].tx += sent;
				rte_pktmbuf_free(*(struct rte_mbuf **)e);
				e = &d;
			}
			// }
			sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
			port_statistics[portid].tx += sent;

			/* if timer is enabled */
			if (timer_period > 0)
			{

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period))
				{
					/* do this only on master core */
					running_second += 1;
					print_stats();
					printf("%d\n", running_second);
					send_rate = (port_statistics[portid].tx * mac_length_send * 8 / running_second) / 1000000000.0;
					receive_rate = (port_statistics[portid].rx * mac_length_receive * 8 / running_second) / 1000000000.0;
					printf("send_rate= %f Gb\nreceive_rate= %f Gb\n", send_rate, receive_rate);
					// printf("pack_err= %d\n", pack_err);
					/* reset the timer */
					timer_tsc = 0;
				}
			}

			prev_tsc = cur_tsc;
		}
	}
}
static void
l2fwd_main_loop_receive(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	FILE *fp;
	unsigned lcore_id;
	unsigned j, portid, nb_rx;
	int package_received = 0;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop receive on lcore  %u\n", lcore_id);

	sem_post(&lcore_receive_prepared);
	sem_wait(&lcore_c_prepared);

	while (!force_quit)
	{
		portid = 0;
		nb_rx = rte_eth_rx_burst((uint8_t)portid, 0, pkts_burst, MAX_PKT_BURST);

		port_statistics[portid].rx += nb_rx;
		// if (package_received < 400)
		// {
		for (j = 0; j < nb_rx; j++)
		{
			print_mbuf_receive(pkts_burst[j]);
			rte_ring_mp_enqueue(ring_receive, pkts_burst[j]);
			//rte_pktmbuf_free(pkts_burst[j]);
			package_received++;
			if (force_quit)
				break;
		}
		// }
		// else
		// 	break;
	}
}

static void
l2fwd_main_producer(void)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop produce on core %u\n", lcore_id);

	uint8_t *adcnt = NULL;
	int cnt = 0;
	int packet_num_threw_in_ring = 0;
	long long int packet_num_want_to_send = 1;
	uint16_t pcnt = 0;
	int indx_seg = 0;

	data_to_be_sent = (uint8_t *)malloc(sizeof(uint8_t) * 1);
	data_to_be_sent[0] = SENDABLE_FLAG;

	uint8_t flag = SENDABLE_FLAG;
	struct rte_mbuf *m;

	int8_t type;
	int16_t num;
	int16_t length;

	sem_post(&lcore_p_prepared);
	sem_wait(&lcore_send_prepared);

	while (!force_quit)
	{
		if (!rte_ring_full(ring_send))
		{
			m = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
			if (m == NULL)
			{
				printf("mempool已满，mbuf申请失败!%d\n", packet_num_threw_in_ring);
				continue;
			}

			package(data_to_be_sent, 8, m);
			while ((!force_quit) && (rte_ring_mp_enqueue(ring_send, m) < 0))
				; //printf("p!\n");

			packet_num_threw_in_ring++;
			feedbackable = 0;
		}
		// }
	}
	printf("入列的包个数%d\n", packet_num_threw_in_ring);
}
static void
l2fwd_main_consumer(void)
{
	void *d = NULL;
	void **e = &d;
	unsigned lcore_id;
	uint8_t *adcnt = NULL;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop consume on core %u\n", lcore_id);
	int indx_seg = 0;
	int pkg_cnt = 0;
	buff_empty = 1;

	sem_post(&lcore_c_prepared);
	sem_wait(&lcore_receive_prepared);
	sem_wait(&rx_prepared);

	while (!force_quit)
	{
		if (!buff_empty)
			sem_wait(&sem_buff_empty);
		if (buff_empty)
		{
			if (rte_ring_mc_dequeue(ring_receive, e) < 0)
				;
			else
			{
				adcnt = rte_pktmbuf_mtod(*(struct rte_mbuf **)e, uint8_t *);

				pthread_mutex_lock(&mutex_startNum_rx);
				startNum_rx++;
				pthread_mutex_unlock(&mutex_startNum_rx);

				depackage(&package_rx[index_rx_write], adcnt);

				// indx_seg++;
				// if (indx_seg >= SEG_SIZE)
				// {
				// 	indx_seg = 0;
				// 	pthread_mutex_lock(&mutex_buff_empty);
				// 	buff_empty = 0;
				// 	pthread_mutex_unlock(&mutex_buff_empty);
				// 	sem_post(&sem_buff_full);
				// 	printf("DPDK_pkg_cnt:%d\n", pkg_cnt++);
				// }
				pthread_mutex_lock(&mutex_send_token);
				send_token++;
				pthread_mutex_unlock(&mutex_send_token);
				rte_pktmbuf_free(*(struct rte_mbuf **)e);
			}
		}
	}
	// 结束后叫醒
	// sem_post(&feedback_sem);
	// sem_post(&sem_buff_full);
	sem_post(&cache_rx);
}

static int
l2fwd_launch_one_lcore_send(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop_send();
	return 0;
}
static int
l2fwd_launch_one_lcore_receive(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop_receive();
	return 0;
}
l2fwd_launch_one_lcore_p(__attribute__((unused)) void *dummy)
{
	l2fwd_main_producer();
	return 0;
}
l2fwd_launch_one_lcore_c(__attribute__((unused)) void *dummy)
{
	l2fwd_main_consumer();
	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90  /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++)
	{
		if (force_quit)
			return;
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++)
		{
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1)
			{
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						   "Mbps - %s\n",
						   (uint8_t)portid,
						   (unsigned)link.link_speed,
						   (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						   (uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN)
			{
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0)
		{
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1))
		{
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM)
	{
		printf("\n\nSignal %d received, preparing to exit...\n",
			   signum);
		force_quit = true;
	}
}

int main(int argc, char **argv)
{
	feedbackable = 1;
	int ret;
	uint8_t nb_ports;
	uint8_t portid;
	unsigned lcore_id;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
												 MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
												 rte_socket_id());
	printf("RTE_MBUF_DEFAULT_BUF_SIZE:%d\n", RTE_MBUF_DEFAULT_BUF_SIZE);
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	printf("mempool init done\n");

	/* create ring */
	ring_send = rte_ring_create("RING_SEND", 1024, rte_socket_id(), 0);
	if (ring_send == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_send\n");
	printf("ring_send create done\n");

	ring_receive = rte_ring_create("RING_receive", 1024, rte_socket_id(), 0);
	if (ring_receive == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_receive\n");
	printf("ring_receive create done\n");

	//check ethernet ports
	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
	else
		printf("number of Ethernet ports that are available:%d\n", rte_eth_dev_count());

	/* Initialise each port */
	portid = 0;
	/* init port */
	printf("Initializing port %u... ", (unsigned)portid);
	fflush(stdout);
	ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, (unsigned)portid);

	rte_eth_macaddr_get(portid, &l2fwd_ports_eth_addr[portid]);

	/* init one RX queue */
	fflush(stdout);
	ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
								 rte_eth_dev_socket_id(portid),
								 NULL,
								 l2fwd_pktmbuf_pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				 ret, (unsigned)portid);

	/* init one TX queue on each port */
	fflush(stdout);
	ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
								 rte_eth_dev_socket_id(portid),
								 NULL);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				 ret, (unsigned)portid);

	/* Initialize TX buffers */
	tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
										   RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
										   rte_eth_dev_socket_id(portid));
	if (tx_buffer[portid] == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
				 (unsigned)portid);

	rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
											 rte_eth_tx_buffer_count_callback,
											 &port_statistics[portid].dropped);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot set error callback for "
							   "tx buffer on port %u\n",
				 (unsigned)portid);

	/* Start device */
	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				 ret, (unsigned)portid);

	printf("done: \n");

	rte_eth_promiscuous_enable(portid);

	printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
		   (unsigned)portid,
		   l2fwd_ports_eth_addr[portid].addr_bytes[0],
		   l2fwd_ports_eth_addr[portid].addr_bytes[1],
		   l2fwd_ports_eth_addr[portid].addr_bytes[2],
		   l2fwd_ports_eth_addr[portid].addr_bytes[3],
		   l2fwd_ports_eth_addr[portid].addr_bytes[4],
		   l2fwd_ports_eth_addr[portid].addr_bytes[5]);

	/* initialize port stats */
	memset(&port_statistics, 0, sizeof(port_statistics));

	check_all_ports_link_status(1, 0x1);

	/* for TaskScheduler */
	/* 初始化信号量 */
	sem_init(&lcore_send_prepared, 0, 0);
	sem_init(&lcore_receive_prepared, 0, 0);
	sem_init(&lcore_p_prepared, 0, 0);
	sem_init(&lcore_c_prepared, 0, 0);
	// sem_init(&feedback_sem, 0, 0);

	/* 初始化互斥锁 */
	pthread_mutex_init(&mutex1_tx, NULL);
	pthread_mutex_init(&mutex2_tx, NULL);
	pthread_mutex_init(&mutex1_rx, NULL);
	pthread_mutex_init(&mutex2_rx, NULL);
	pthread_mutex_init(&mutex3_rx, NULL);

	/* 初始化信号量 */
	sem_init(&tx_can_be_destroyed, 0, 0);
	sem_init(&rx_can_be_destroyed, 0, 0);
	sem_init(&tx_buff_can_be_destroyed, 0, 0);
	sem_init(&rx_buff_can_be_destroyed, 0, 0);
	sem_init(&tx_prepared, 0, 0);
	sem_init(&rx_prepared, 0, 0);
	sem_init(&tx_buff_prepared, 0, 0);
	sem_init(&rx_buff_prepared, 0, 0);
	sem_init(&cache_tx, 0, 0);
	sem_init(&cache_rx, 0, 0);
	sem_init(&sem_buff_full, 0, 0);
	sem_init(&sem_buff_empty, 0, 0);

	/* 初始化线程池 */
	// pool_init(0, 1, 0);
	// printf("creat pool 0...\n");
	// pool_init(1, 1, 1);
	// printf("creat pool 1...\n");
	// pool_init(2, threadNum_tx, 2);
	// printf("creat pool 2...\n");
	// pool_init(2 + threadNum_tx, threadNum_rx, 3);
	// printf("creat pool 3...\n");
	// pool_init(2 + threadNum_tx + threadNum_rx, 1, 4);
	// printf("creat pool 4...\n");
	// pool_init(3 + threadNum_tx + threadNum_rx, 1, 5);
	// printf("creat pool 4...\n");

	pool_init(0, 1, 1);
	printf("creat pool 1...\n");
	pool_init(1, threadNum_rx, 3);
	printf("creat pool 3...\n");
	// pool_init(1 + threadNum_rx, 1, 5);
	// printf("creat pool 5...\n");

	/* 添加发送端主任务 */
	// pool_add_task(TaskScheduler_tx, NULL, 0);
	// printf("add Tx TaskScheduler to pool 0...\n");
	/* 添加接收端主任务 */
	pool_add_task(TaskScheduler_rx, NULL, 1);
	printf("add Rx TaskScheduler to pool 1...\n");
	/* 添加发送端缓存任务 */
	// pool_add_task(Tx_buff, NULL, 4);
	// printf("add Tx Buff to pool 4...\n");
	/* 添加发送端缓存任务 */
	// pool_add_task(Rx_buff, NULL, 5);
	// printf("add Rx Buff to pool 5...\n");

	ret = 0;
	/* launch tasks on lcore */
	rte_eal_remote_launch(l2fwd_launch_one_lcore_c, NULL, 1);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_receive, NULL, 2);
	// sleep(10);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_p, NULL, 3);
	// sleep(3);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_send, NULL, 4);

	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		if (rte_eal_wait_lcore(lcore_id) < 0)
		{
			ret = -1;
			break;
		}
	}

	portid = 0;

	/* 等待信号销毁线程 */
	// sem_wait(&tx_can_be_destroyed);
	// pool_destroy(0);
	sem_wait(&rx_can_be_destroyed);
	pool_destroy(1);
	// sem_wait(&tx_buff_can_be_destroyed);
	// pool_destroy(4);
	// sem_wait(&rx_buff_can_be_destroyed);
	// pool_destroy(5);

	/* 销毁信号量*/
	// sem_destroy(&tx_can_be_destroyed);
	sem_destroy(&rx_can_be_destroyed);
	// sem_destroy(&tx_buff_can_be_destroyed);
	// sem_destroy(&rx_buff_can_be_destroyed);
	// sem_destroy(&tx_prepared);
	sem_destroy(&rx_prepared);
	// sem_destroy(&tx_buff_prepared);
	// sem_destroy(&rx_buff_prepared);
	// sem_destroy(&cache_tx);
	sem_destroy(&cache_rx);
	sem_destroy(&sem_buff_full);
	sem_destroy(&sem_buff_empty);

	printf("Closing port %d...", portid);
	rte_eth_dev_stop(portid);
	rte_eth_dev_close(portid);
	printf(" Done\n");

	printf("Bye...\n");
	print_stats();
	// printf("else1=%d,else2=%d\n", else1, else2);
	return ret;
}
