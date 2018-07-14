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

#include "info_pkg_head.h"
#include "udp_head.h"

static volatile bool force_quit;

#define PKG_LEN (MAX_PKG_LEN + MAC_HEAD_LEN + UDP_HEAD_LEN + IP_HEAD_LEN + INFO_PKG_HEAD_LEN)

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF 8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 4 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
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
struct rte_ring *ring_recieve;
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
static void print_stats(void)
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
static void print_mbuf_send(struct rte_mbuf *m)
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
static void print_mbuf_recieve(struct rte_mbuf *m)
{
	uint8_t *adcnt;
	FILE *fp;
	fp = fopen("recieve_data.txt", "a");
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
int read_from_txt(char *a, int num, uint8_t *data_to_be_sent) //从文件中将数据读入全局数组b[]中，返回0读取成功，返回1文件不存在
{
	FILE *fp = NULL;
	int i = 0;
	fp = fopen(a, "r");
	if (fp == NULL)
	{
		printf("此文件不存在");
		return 1;
	}
	for (i = 0; i < num; i++)
		fscanf(fp, "%x  ", &data_to_be_sent[i]);
	for (i = 0; i < num; i++)
		printf("%d   %x\n", i, data_to_be_sent[i]);
	fclose(fp);
	return 0;
}

unsigned char data_to_be_sent[2048] = {0};
unsigned char packet_to_be_sent[2048] = {0};
unsigned char data_recieved[2048] = {0};
volatile uint8_t send_en = 0x00; //此变量后面改为全局变量 00停止发送  01开始发送
int pack_err = 0;
int num_err_pkg = 0;
int num_data_nvld = 0;

int package(mac_hdr *mhdr, ip_hdr *ihdr,
			udp_fhdr_hdr *uhdr,
			info_pkg_head_t *h,
			unsigned char *data,
			struct rte_mbuf *m)
{
	uint8_t *adcnt = NULL;
	int total_length = 42 + 16 + h->pack_len;

	if (total_length > 59)
		rte_pktmbuf_append(m, total_length);
	else
		rte_pktmbuf_append(m, 60);

	adcnt = rte_pktmbuf_mtod(m, uint8_t *);

	load_udp_head(mhdr, ihdr, uhdr, adcnt); // checksum位置0
	adcnt += 42;

	load_info_pkg_head_dl(h, adcnt);
	adcnt += 16;

	for (int i = 0; i < h->pack_len; i++)
	{
		*adcnt = data[i];
		*adcnt++;
	}

	return total_length;
}

static int l2fwd_main_loop_send(void)
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

	double recieve_rate = 0;
	double send_rate = 0;
	int mac_length_send = PKG_LEN;
	int mac_length_recieve = PKG_LEN;

	RTE_LOG(INFO, L2FWD, "entering main loop send on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++)
	{

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
				portid);
	}

	while (!force_quit)
	{

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (diff_tsc > drain_tsc)
		{
			portid = 0;
			buffer = tx_buffer[portid];

			// for (j = 0; j < 4; j++)
			// {
			if (rte_ring_mc_dequeue(ring_send, e) < 0)
				;
			else
			{
				print_mbuf_send(*(struct rte_mbuf **)e);
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
					recieve_rate = (port_statistics[portid].rx * mac_length_recieve * 8 / running_second) / 1000000000.0;
					printf("send_rate= %f Gb\nrecieve_rate= %f Gb\n", send_rate, recieve_rate);
					printf("pack_err= %d\n", pack_err);
					/* reset the timer */
					timer_tsc = 0;
				}
			}

			prev_tsc = cur_tsc;
		}
	}
}

static void
l2fwd_main_loop_recieve(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	FILE *fp;
	unsigned lcore_id;
	unsigned j, portid, nb_rx;
	int package_recieved = 0;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop recieve on lcore  %u\n", lcore_id);

	while (!force_quit)
	{
		portid = 0;
		nb_rx = rte_eth_rx_burst((uint8_t)portid, 0, pkts_burst, MAX_PKT_BURST);

		port_statistics[portid].rx += nb_rx;
		// if (package_recieved < 400)
		// {
		for (j = 0; j < nb_rx; j++)
		{
			print_mbuf_recieve(pkts_burst[j]);
			rte_ring_mp_enqueue(ring_recieve, pkts_burst[j]);
			//rte_pktmbuf_free(pkts_burst[j]);
			package_recieved++;
		}
		// }
		// else
		// 	break;
	}
}

static void
l2fwd_main_p(void)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop produce on core %u\n", lcore_id);

	uint8_t *adcnt = NULL;
	int packet_num_threw_in_ring = 0;
	int datalength = MAX_PKG_LEN + INFO_PKG_HEAD_LEN + MAC_HEAD_LEN;

	int total_length = 0;

	mac_hdr mhdr;
	init_mac_hdr(&mhdr);

	udp_fhdr_hdr uhdr;
	init_udp_fhdr_hdr(&uhdr, datalength);

	ip_hdr ihdr;
	init_ip_hdr(&ihdr, uhdr.length);

	info_pkg_head_t h;
	init_dl_info_pkg_head(&h);

	for (int i = 0; i < MAX_PKG_LEN; i++)
		data_to_be_sent[i] = 0xFF;

	while (!force_quit)
	{
		if (unlikely(send_en))
			if (!rte_ring_full(ring_send))
			{
				struct rte_mbuf *m;
				m = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
				if (m == NULL)
					printf("mempool已满，mbuf申请失败!%d\n", packet_num_threw_in_ring);
				else
				{
					total_length = package(&mhdr, &ihdr, &uhdr, &h, data_to_be_sent, m);
					while ((!force_quit) && (rte_ring_mp_enqueue(ring_send, m) < 0))
						;
					packet_num_threw_in_ring++;
				}
			}
	}
	printf("入列的包个数%d\n", packet_num_threw_in_ring);
}
static void
l2fwd_main_c(void)
{
	void *d = NULL;
	void **e = &d;
	unsigned lcore_id;
	uint8_t *adcnt = NULL;

	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop consume on core %u\n", lcore_id);

	info_pkg_head_t h_t, ph_t;
	info_pkg_head_t *h = &h_t;
	info_pkg_head_t *ph = &ph_t;
	init_pre_info_pkg_head(ph);

	int err = 0;

	while (!force_quit)
	{
		if (rte_ring_mc_dequeue(ring_recieve, e) < 0)
			;
		//printf("!\n");
		else
		{
			adcnt = rte_pktmbuf_mtod(*(struct rte_mbuf **)e, uint8_t *);
			adcnt += 38;
			// data_lesngth = ((*adcnt) << 8) + (*(adcnt + 1)) - 8;
			adcnt += 4;

			send_en = *(adcnt);
			h->data_vld = *(adcnt + 1);
			if (h->data_vld)
			{
				h->pack_type = *(adcnt + 2);
				h->pack_hd = ((*(adcnt + 3)) << 8) + (*(adcnt + 4));
				h->pack_idx = ((*(adcnt + 5)) << 8) + (*(adcnt + 6));
				h->pack_len = ((*(adcnt + 7)) << 8) + (*(adcnt + 8));
				adcnt += 16;
				if (check_info_pkg_head(h, ph))
					write_data_pkg(h, adcnt);
				else
					num_err_pkg++;
			}
			else
				num_data_nvld++;

			rte_pktmbuf_free(*(struct rte_mbuf **)e);
		}
	}
}
static int
l2fwd_launch_one_lcore_send(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop_send();
	return 0;
}
static int
l2fwd_launch_one_lcore_recieve(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop_recieve();
	return 0;
}
l2fwd_launch_one_lcore_p(__attribute__((unused)) void *dummy)
{
	l2fwd_main_p();
	return 0;
}
l2fwd_launch_one_lcore_c(__attribute__((unused)) void *dummy)
{
	l2fwd_main_c();
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
	l2fwd_pktmbuf_pool =
		rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
								MEMPOOL_CACHE_SIZE, 0,
								RTE_MBUF_DEFAULT_BUF_SIZE,
								rte_socket_id());
	printf("RTE_MBUF_DEFAULT_BUF_SIZE:%d\n", RTE_MBUF_DEFAULT_BUF_SIZE);
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	printf("mempool init done\n");

	ring_send = rte_ring_create("RING_SEND", 1024, rte_socket_id(), 0);
	if (ring_send == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_send\n");
	printf("ring_send create done\n");

	ring_recieve = rte_ring_create("RING_RECIEVE", 1024, rte_socket_id(), 0);
	if (ring_recieve == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_recieve\n");
	printf("ring_recieve create done\n");

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

	ret = 0;
	/* launch tasks on lcore */
	rte_eal_remote_launch(l2fwd_launch_one_lcore_c, NULL, 1);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_recieve, NULL, 2);
	sleep(10);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_p, NULL, 3);
	sleep(3);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_send, NULL, 4);

	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		if (rte_eal_wait_lcore(lcore_id) < 0)
		{
			ret = -1;
			break;
		}
	}

	// FILE *fp;
	// FILE *fpp;
	// struct rte_mbuf *m;
	// m=rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);

	// uint8_t * adcnt;

	// for(adcnt=(uint8_t*)m->buf_addr;adcnt<(uint8_t*)rte_pktmbuf_mtod(m,uint8_t*);adcnt++)
	// 	*adcnt=0xaa;
	// for(adcnt=rte_pktmbuf_mtod(m,uint8_t*);adcnt<rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt++)
	// 	*adcnt=0xaa;
	// for(adcnt=rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt<(uint8_t*)m->buf_addr+m->buf_len;adcnt++)
	// 	*adcnt=0xaa;

	// fp=fopen("status","w");
	// struct rte_ring *r = rte_ring_create("MY_RING", 1024,rte_socket_id(), 0);
	// rte_ring_dump (fp, r);
	// rte_ring_mp_enqueue(r,m);
	// rte_ring_dump (fp, r);
	// rte_ring_mc_dequeue(r,e);
	// rte_ring_dump (fp, r);
	// fclose(fp);
	// print_mbuf_send(*(struct rte_mbuf **)e,fpp);
	// rte_pktmbuf_free(*(struct rte_mbuf **)e);
	// print_mbuf_send(*(struct rte_mbuf **)e,fpp);

	portid = 0;

	printf("Closing port %d...", portid);
	rte_eth_dev_stop(portid);
	rte_eth_dev_close(portid);
	printf(" Done\n");

	printf("Bye...\n");
	print_stats();
	printf("num_err_pkg=%d,num_data_nvld=%d\n", num_err_pkg, num_data_nvld);
	return ret;
}
