//版本号:2.2

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

#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <complex.h>
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
sem_t buff_full;

/* 声明信号量 */
sem_t lcore_send_prepared;
sem_t lcore_receive_prepared;
sem_t lcore_p_prepared;
sem_t lcore_c_prepared;
sem_t sendable_sem;

extern uint8_t *databuff;
extern pthread_mutex_t mutex_buff_empty;
extern int buff_empty;
extern int readyNum_tx;
extern int startNum_tx;
extern int index_tx_read;
extern pthread_mutex_t mutex_readyNum_tx;
extern pthread_mutex_t mutex_startNum_tx;
extern struct package_t package_tx[PACK_CACHE];

#define SENDABLE_FLAG 0xFF
#define SEND_TOKEN_INIT 32

int send_flag;
volatile int send_token = SEND_TOKEN_INIT;
pthread_mutex_t mutex_send_token;

// static volatile bool force_quit;	// 不能在其他编译单元内使用
volatile bool force_quit;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MBUFF_DATA_LENGTH 1200
#define SEG_SIZE 1792

// uint8_t *data_to_be_sent;

#define NB_MBUF 8192

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

struct mac_hdr
{
	unsigned char dst_mac[6];
	unsigned char src_mac[6];
	unsigned char type0;
	unsigned char type1;
};
struct ip_hdr
{

	unsigned short version_headlen_tos;
	unsigned short length; //ip包总长度，包含ip头

	unsigned short flags0;
	unsigned short flags1; //标识-标志-片偏移

	unsigned short ttl_protocol_checksum0;
	unsigned short ttl_protocol_checksum1; //ttl-协议-校验

	unsigned short src_ip0;
	unsigned short src_ip1; //源ip

	unsigned short dst_ip0;
	unsigned short dst_ip1; //目的ip
};
struct udp_fhdr_hdr
{

	unsigned short src_ip0;
	unsigned short src_ip1; //源ip

	unsigned short dst_ip0;
	unsigned short dst_ip1; //目的ip
	unsigned short reserved;

	unsigned short src_port;

	unsigned short dst_port;

	unsigned short length;

	unsigned short checksum;
};

unsigned short cal_ip_checksum(struct ip_hdr hdr)
{
	unsigned long checksum = 0;
	checksum =
		hdr.version_headlen_tos + hdr.length + hdr.flags0 + hdr.flags1 + hdr.ttl_protocol_checksum0 + hdr.ttl_protocol_checksum1 + hdr.src_ip0 + hdr.src_ip1 + hdr.dst_ip0 + hdr.dst_ip1;
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	return (unsigned short)(~checksum);
}
unsigned short cal_udp_checksum(struct udp_fhdr_hdr hdr) //, unsigned char *buffer)
{
	unsigned long checksum = 0;
	// int num = hdr.length - 8;
	checksum =
		hdr.src_ip0 + hdr.src_ip1 + hdr.dst_ip0 + hdr.dst_ip1 + hdr.reserved + hdr.length + hdr.src_port + hdr.dst_port + hdr.length + hdr.checksum;
	// while (num > 1)
	// {
	// 	checksum += (*buffer << 8) + *(buffer + 1);
	// 	buffer += 2;
	// 	num -= sizeof(short);
	// }
	// if (num)
	// {
	// 	checksum += *(unsigned short *)buffer << 8;
	// }
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	return (unsigned short)(~checksum);
}
unsigned short get_checksum(unsigned short *buffer, int size) //size是字节数
{
	unsigned long cksum = 0;
	while (size > 1)
	{
		printf("size=%d  buffer=%x\n", size, *buffer);
		cksum += *buffer;
		buffer++;
		size -= sizeof(short);
		printf("size=%d  checksum=%x\n", size, cksum);
	}
	if (size)
	{
		cksum += *(unsigned short *)buffer << 8;
		printf("size=%d  buffer=%x\n", 1, *buffer);
		printf("size=%d  checksum=%x\n", size, cksum);
	}
	printf("checksum00=%x\n", cksum);
	cksum = (cksum >> 16) + (cksum & 0xffff);
	printf("+ checksum11=%x\n", cksum);
	cksum += (cksum >> 16);
	printf("= checksum22=%x\n", cksum);
	return (unsigned short)(~cksum);
}

/* 从Tx包缓冲区直接打包 */
int package(struct mac_hdr mhdr, struct ip_hdr ihdr, struct udp_fhdr_hdr uhdr, struct package_t *pkg_tx, int8_t type, int16_t num, int16_t length, struct rte_mbuf *m)
{

	unsigned short ichecksum = cal_ip_checksum(ihdr);
	unsigned short uchecksum = cal_udp_checksum(uhdr); //, data);

	// static const int length_prefix = 64;
	static const int length_type = sizeof(type);
	static const int length_num = sizeof(num);
	static const int length_length = sizeof(length);

	int total_length = 42 + length + length_type + length_num + length_length;

	if (total_length > 59)
		rte_pktmbuf_append(m, total_length);
	else
		rte_pktmbuf_append(m, 60);

	uint8_t *adcnt = NULL;

	for (adcnt = (uint8_t *)m->buf_addr; adcnt < (uint8_t *)rte_pktmbuf_mtod(m, uint8_t *); adcnt++)
		*adcnt = 0x00;

	adcnt = rte_pktmbuf_mtod(m, uint8_t *);

	*adcnt = mhdr.dst_mac[0];
	adcnt++;
	*adcnt = mhdr.dst_mac[1];
	adcnt++;
	*adcnt = mhdr.dst_mac[2];
	adcnt++;
	*adcnt = mhdr.dst_mac[3];
	adcnt++;
	*adcnt = mhdr.dst_mac[4];
	adcnt++;
	*adcnt = mhdr.dst_mac[5];
	adcnt++;

	*adcnt = mhdr.src_mac[0];
	adcnt++;
	*adcnt = mhdr.src_mac[1];
	adcnt++;
	*adcnt = mhdr.src_mac[2];
	adcnt++;
	*adcnt = mhdr.src_mac[3];
	adcnt++;
	*adcnt = mhdr.src_mac[4];
	adcnt++;
	*adcnt = mhdr.src_mac[5];
	adcnt++;

	*adcnt = mhdr.type0;
	adcnt++;
	*adcnt = mhdr.type1;
	adcnt++;

	*adcnt = ihdr.version_headlen_tos >> 8;
	adcnt++;
	*adcnt = ihdr.version_headlen_tos;
	adcnt++;
	*adcnt = ihdr.length >> 8;
	adcnt++;
	*adcnt = ihdr.length;
	adcnt++;
	*adcnt = ihdr.flags0 >> 8;
	adcnt++;
	*adcnt = ihdr.flags0;
	adcnt++;
	*adcnt = ihdr.flags1 >> 8;
	adcnt++;
	*adcnt = ihdr.flags1;
	adcnt++;
	*adcnt = ihdr.ttl_protocol_checksum0 >> 8;
	adcnt++;
	*adcnt = ihdr.ttl_protocol_checksum0;
	adcnt++;
	*adcnt = ichecksum >> 8;
	adcnt++;
	*adcnt = ichecksum;
	adcnt++;
	*adcnt = ihdr.src_ip0 >> 8;
	adcnt++;
	*adcnt = ihdr.src_ip0;
	adcnt++;
	*adcnt = ihdr.src_ip1 >> 8;
	adcnt++;
	*adcnt = ihdr.src_ip1;
	adcnt++;
	*adcnt = ihdr.dst_ip0 >> 8;
	adcnt++;
	*adcnt = ihdr.dst_ip0;
	adcnt++;
	*adcnt = ihdr.dst_ip1 >> 8;
	adcnt++;
	*adcnt = ihdr.dst_ip1;
	adcnt++;

	*adcnt = uhdr.src_port >> 8;
	adcnt++;
	*adcnt = uhdr.src_port;
	adcnt++;
	*adcnt = uhdr.dst_port >> 8;
	adcnt++;
	*adcnt = uhdr.dst_port;
	adcnt++;
	*adcnt = uhdr.length >> 8;
	adcnt++;
	*adcnt = uhdr.length;
	adcnt++;
	*adcnt = uchecksum >> 8;
	adcnt++;
	*adcnt = uchecksum;
	adcnt++;

	// 填写包信息
	*adcnt = type;
	adcnt++;
	*adcnt = num >> 8;
	adcnt++;
	*adcnt = (int8_t)num;
	adcnt++;
	*adcnt = length >> 8;
	adcnt++;
	*adcnt = (int8_t)length;
	adcnt++;

	if (type == 0)
	{
		uint8_t *tbs_p = (uint8_t *)(pkg_tx->tbs);
		for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
		{
			*adcnt = *tbs_p;
			adcnt++;
			tbs_p++;
		}

		uint8_t *cqi_p = (uint8_t *)(pkg_tx->CQI_index);
		for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
		{
			*adcnt = *cqi_p;
			adcnt++;
			cqi_p++;
		}

		uint8_t *snr_p = (uint8_t *)(&(pkg_tx->SNR));
		for (int i = 0; i < sizeof(float); i++)
		{
			*adcnt = *snr_p;
			adcnt++;
			snr_p++;
		}
	}
	else if (type == 1)
	{
		int8_t *data = (int8_t *)pkg_tx->y + length * num;
		for (int cnt = 0; cnt < length; cnt++)
		{
			*adcnt = data[cnt];
			adcnt++;
		}
	}
	else if (type >= 2 && type < 2 + MAX_BEAM)
	{
		int num_data = type - 2;
		int8_t *data = (int8_t *)pkg_tx->data[num_data] + length * num;
		for (int cnt = 0; cnt < length; cnt++)
		{
			*adcnt = data[cnt];
			adcnt++;
		}
	}

	for (adcnt; adcnt < rte_pktmbuf_mtod(m, uint8_t *) + m->data_len; adcnt++)
		*adcnt = 0xee;

	for (adcnt = rte_pktmbuf_mtod(m, uint8_t *) + m->data_len; adcnt < (uint8_t *)m->buf_addr + m->buf_len; adcnt++)
		*adcnt = 0x00;

	return total_length;
}

static void
l2fwd_main_loop_send(void)
{

	void *d = NULL;
	void **e = &d;
	// struct rte_mbuf **e;
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
		if (unlikely(diff_tsc > drain_tsc))
		{
			portid = 0;
			buffer = tx_buffer[portid];

			for (j = 0; j < 4; j++)
			{
				if (rte_ring_mc_dequeue(ring_send, e) < 0)
					;
				else
				{
					// printf("send_token:%d", send_token);
					// print_mbuf_send(*(struct rte_mbuf **)e);
					sent = rte_eth_tx_buffer(portid, 0, buffer, *(struct rte_mbuf **)e);
					port_statistics[portid].tx += sent;
					rte_pktmbuf_free(*(struct rte_mbuf **)e);
					e = &d;
				}
			}
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
					// print_stats();
					// printf("%d\n", running_second);
					send_rate = (port_statistics[portid].tx * mac_length_send * 8 / running_second) / 1000000000.0;
					// printf("send_rate= %f Gb\nreceive_rate= %f Gb\n", send_rate, receive_rated
					// timer_tsc = 0;
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
			// print_mbuf_receive(pkts_burst[j]);
			pthread_mutex_lock(&mutex_send_token);
			send_token++;
			pthread_mutex_unlock(&mutex_send_token);
			// rte_ring_mp_enqueue(ring_receive, pkts_burst[j]);
			uint8_t *adcnt = rte_pktmbuf_mtod(pkts_burst[j], uint8_t *);
			// send_flag = *(adcnt + 43);
			// printf("send_flag = %d\n", send_flag);

			rte_pktmbuf_free(pkts_burst[j]);
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
	int indx_seg = 0;

	struct rte_mbuf *m;
	int sendable_cnt = 0;

	// 打包相关参数
	int type = 0;
	int num = 0;
	int length = 4 * 17;

	int datalength = 1200 + 5;

	struct udp_fhdr_hdr uhdr;
	uhdr.src_port = 0xBEE4;
	uhdr.dst_port = 0xBEE4;
	uhdr.length = datalength + 8;
	uhdr.reserved = 0x0011;
	uhdr.src_ip0 = 0xC0A8;
	uhdr.src_ip1 = 0x1405;
	uhdr.dst_ip0 = 0xC0A8;
	uhdr.dst_ip1 = 0x1401;

	struct ip_hdr ihdr;
	ihdr.version_headlen_tos = 0x4500;
	ihdr.length = uhdr.length + 20;

	ihdr.flags0 = 0x0000;
	ihdr.flags1 = 0x4000;

	ihdr.ttl_protocol_checksum0 = 0x4011;
	ihdr.ttl_protocol_checksum1 = 0x0000;

	ihdr.src_ip0 = 0xC0A8;
	ihdr.src_ip1 = 0x1405;

	ihdr.dst_ip0 = 0xC0A8;
	ihdr.dst_ip1 = 0x1401;

	struct mac_hdr mhdr;
	mhdr.dst_mac[0] = 0x68;
	mhdr.dst_mac[1] = 0xCC;
	mhdr.dst_mac[2] = 0x6E;
	mhdr.dst_mac[3] = 0xA4;
	mhdr.dst_mac[4] = 0xBC;
	mhdr.dst_mac[5] = 0x53;

	mhdr.src_mac[0] = 0x68;
	mhdr.src_mac[1] = 0xCC;
	mhdr.src_mac[2] = 0x6E;
	mhdr.src_mac[3] = 0xA4;
	mhdr.src_mac[4] = 0xBC;
	mhdr.src_mac[5] = 0x7F;
	mhdr.type0 = 0x08;
	mhdr.type1 = 0x00;

	send_flag = 1;

	sem_post(&lcore_p_prepared);
	sem_wait(&lcore_send_prepared);
	sem_wait(&tx_prepared);

	while (!force_quit)
	{
		if (readyNum_tx <= 0)
			sem_wait(&cache_tx);
		// if (rte_ring_full(ring_send))
		// 	printf("!");
		// if ((!rte_ring_full(ring_send)) && readyNum_tx > 0 && send_flag)
		if (readyNum_tx > 0 && (send_token > 0))
		// if (rte_ring_empty(ring_send) && readyNum_tx > 0)
		{
			pthread_mutex_lock(&mutex_send_token);
			send_token--;
			pthread_mutex_unlock(&mutex_send_token);

			m = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
			if (m == NULL)
			{
				printf("mempool已满，mbuf申请失败!%d\n", packet_num_threw_in_ring);
				continue;
			}

			// package(databuff + MBUFF_DATA_LENGTH * indx_seg, MBUFF_DATA_LENGTH, m);

			package(mhdr, ihdr, uhdr, &package_tx[index_tx_read], type, num, 1200, m);

			// 打包参数处理
			num++;
			if (type == 0)
			{
				if (num > 0)
				{
					type++;
					num = 0;
					length = CARRIER_NUM;
				}
			}
			else if (type == 1)
			{
				if (num >= sizeof(lapack_complex_float) * RX_ANT_NUM * SYMBOL_NUM)
				{
					num = 0;
					type++;
					length = CARRIER_NUM;
				}
			}
			else
			{
				if (num >= MAX_CQI_MOD * DATA_SYM_NUM)
				{
					num = 0;
					type++;
					if (type >= 2 + MAX_BEAM)
					{
						type = 0;
						length = 4 * 17;
						index_tx_read++;
						if (index_tx_read >= PACK_CACHE)
							index_tx_read = 0;
						pthread_mutex_lock(&mutex_readyNum_tx);
						readyNum_tx--;
						pthread_mutex_unlock(&mutex_readyNum_tx);
						pthread_mutex_lock(&mutex_startNum_tx);
						startNum_tx--;
						pthread_mutex_unlock(&mutex_startNum_tx);
						// printf("p:readyNum_tx:%4d,startNum_tx:%4d\n", readyNum_tx, startNum_tx);
					}
				}
			}

			while ((!force_quit) && (rte_ring_mp_enqueue(ring_send, m) < 0))
				;

			packet_num_threw_in_ring++;

			// // databuff定位
			// indx_seg++;
			// if (indx_seg >= SEG_SIZE)
			// {
			// 	indx_seg = 0;
			// 	pthread_mutex_lock(&mutex_buff_empty);
			// 	buff_empty = 1;
			// 	pthread_mutex_unlock(&mutex_buff_empty);
			// }
		}
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

	sem_post(&lcore_c_prepared);
	sem_wait(&lcore_receive_prepared);

	while (!force_quit)
	{
		if (rte_ring_mc_dequeue(ring_receive, e) < 0)
			;
		else
		{
			// adcnt = rte_pktmbuf_mtod(*(struct rte_mbuf **)e, uint8_t *);
			// adcnt += 42;
			// if (*adcnt == SENDABLE_FLAG)
			// {
			// pthread_mutex_lock(&mutex_send_token);
			// send_token = SEND_TOKEN_INIT;
			// pthread_mutex_unlock(&mutex_send_token);
			// }
			rte_pktmbuf_free(*(struct rte_mbuf **)e);
		}
	}
	// 结束后叫醒
	// sem_post(&sendable_sem);
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
static int
l2fwd_launch_one_lcore_p(__attribute__((unused)) void *dummy)
{
	l2fwd_main_producer();
	return 0;
}
static int
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
	// sendable = 1;
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

	/* 初始化信号量 */
	sem_init(&lcore_send_prepared, 0, 0);
	sem_init(&lcore_receive_prepared, 0, 0);
	sem_init(&lcore_p_prepared, 0, 0);
	sem_init(&lcore_c_prepared, 0, 0);

	/* 初始化互斥锁 */
	pthread_mutex_init(&mutex1_tx, NULL);
	pthread_mutex_init(&mutex2_tx, NULL);
	pthread_mutex_init(&mutex1_rx, NULL);
	pthread_mutex_init(&mutex2_rx, NULL);
	pthread_mutex_init(&mutex3_rx, NULL);

	pthread_mutex_init(&mutex_send_token, NULL);

	/* for TaskScheduler */
	/* 初始化信号量 */
	sem_init(&tx_can_be_destroyed, 0, 0);
	sem_init(&rx_can_be_destroyed, 0, 0);
	// sem_init(&tx_buff_can_be_destroyed, 0, 0);
	// sem_init(&rx_buff_can_be_destroyed, 0, 0);
	sem_init(&tx_prepared, 0, 0);
	sem_init(&rx_prepared, 0, 0);
	// sem_init(&tx_buff_prepared, 0, 0);
	// sem_init(&rx_buff_prepared, 0, 0);
	sem_init(&cache_tx, 0, 0);
	// sem_init(&cache_rx, 0, 0);
	// sem_init(&buff_full, 0, 0);

	/* 初始化线程池 */
	pool_init(0, 1, 0);
	printf("creat pool 0...\n");
	pool_init(1, threadNum_tx, 2);
	printf("creat pool 2...\n");
	// pool_init(1 + threadNum_tx, 1, 4);
	// printf("creat pool 4...\n");

	/* 添加发送端主任务 */
	pool_add_task(TaskScheduler_tx, NULL, 0);
	printf("add Tx TaskScheduler to pool 0...\n");

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
	sem_wait(&tx_can_be_destroyed);
	pool_destroy(0);
	// sem_wait(&rx_can_be_destroyed);
	// pool_destroy(1);
	// sem_wait(&tx_buff_can_be_destroyed);
	// pool_destroy(4);
	// sem_wait(&rx_buff_can_be_destroyed);
	// pool_destroy(5);

	/* 销毁信号量*/
	sem_destroy(&tx_can_be_destroyed);
	// sem_destroy(&rx_can_be_destroyed);
	// sem_destroy(&tx_buff_can_be_destroyed);
	// sem_destroy(&rx_buff_can_be_destroyed);
	sem_destroy(&tx_prepared);
	// sem_destroy(&rx_prepared);
	sem_destroy(&tx_buff_prepared);
	// sem_destroy(&rx_buff_prepared);
	sem_destroy(&cache_tx);
	// sem_destroy(&cache_rx);
	// sem_destroy(&buff_full);

	printf("Closing port %d...", portid);
	rte_eth_dev_stop(portid);
	rte_eth_dev_close(portid);
	printf(" Done\n");

	printf("Bye...\n");
	print_stats();
	// printf("else1=%d,else2=%d\n", else1, else2);
	return ret;
}
