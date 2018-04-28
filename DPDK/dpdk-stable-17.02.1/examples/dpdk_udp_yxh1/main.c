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
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include <pthread.h>


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

#pragma pack(1)

static volatile bool force_quit;


#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 5 /* TX drain every ~5us */
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
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool * l2fwd_pktmbuf_pool = NULL;
struct rte_ring *ring_send ;
struct rte_ring *ring_recieve ;
struct rte_ring *ring_packing;
struct rte_ring *ring_unpacking;
/* Per-port statistics struct */
struct l2fwd_port_statistics {
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

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");
	portid=0;
		/* skip disabled ports */
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\n====================================================\n");
}
static void print_mbuf_send(struct rte_mbuf *m)
{
	uint8_t* adcnt;
	FILE*fp;
	fp=fopen("send_data","a");
	fprintf(fp,"buf_addr:%d\n", m->buf_addr);
	fprintf(fp,"pkt_len:%d\n", m->pkt_len);
	fprintf(fp,"data_len:%d\n", m->data_len);
	fprintf(fp,"buf_len:%d\n", m->buf_len);
	fprintf(fp,"rte_pktmbuf_mtod(m):%d\n", rte_pktmbuf_mtod(m,void*));
	for(adcnt=(uint8_t*)m->buf_addr;adcnt<(uint8_t*)m->buf_addr+m->buf_len;adcnt++)
		fprintf(fp,"%02X ",*adcnt);
	fprintf(fp,"over\n");
	fclose(fp);
}
static void print_mbuf_recieve(struct rte_mbuf *m)
{
	uint8_t* adcnt;
	FILE*fp;
	fp=fopen("recieve_data","a");
	fprintf(fp,"buf_addr:%d\n", m->buf_addr);
	fprintf(fp,"pkt_len:%d\n", m->pkt_len);
	fprintf(fp,"data_len:%d\n", m->data_len);
	fprintf(fp,"buf_len:%d\n", m->buf_len);
	fprintf(fp,"rte_pktmbuf_mtod(m):%d\n", rte_pktmbuf_mtod(m,void*));
	for(adcnt=(uint8_t*)m->buf_addr;adcnt<(uint8_t*)m->buf_addr+m->buf_len;adcnt++)
		fprintf(fp,"%02X ",*adcnt);
	fprintf(fp,"over\n");
	fclose(fp);
}


uint8_t data_to_be_sent[512][2048]={0}; //max = 385
unsigned char packet_to_be_sent[2048]={0};
unsigned char data_recieved[2048]={0};
uint8_t send_en=0x00;//此变量后面改为全局变量 00停止发送  01开始发送
uint64_t udp_package_lost = 0;//udp丢包数目
uint64_t data_package_lost = 0;//数据包丢包数目

const int packLen = 1416;
const int DataMaxLen_Rx = 1400 * 12;  //最多12个包组成
const int DataMaxLen_Tx = 1400 * 77;  //最多77个包组成

const int queLen = 16;
const int packDataLen = 1400;
const int packHeadLen = 16;

const int PackMaxNumTx = 77;  //106440 bytes < 1400 * 77

struct DataPack{
	int length;
	uint8_t *data;
};

struct DataDisplay{
	float trthrou;
	float rethrou;
	float resnr;
	float constellation[2048][2];
	float chan[2048][2];
	uint16_t beam[360];
	uint16_t res[250];
};

struct DataPack *data_pack_Rx; //接收机的接收数据包

struct DataPack data_pack_Tx_forward;
struct DataPack data_pack_Tx_backward;


//发送给显示界面的数据
uint16_t recvbuf[1024];
struct DataDisplay sendbuf;

struct mac_hdr
{
	unsigned char dst_mac[6] ;
	unsigned char src_mac[6] ;
	unsigned char type0;
	unsigned char type1;
};
struct ip_hdr
{
	
	unsigned short version_headlen_tos;
	unsigned short length;//ip包总长度，包含ip头

	unsigned short flags0; unsigned short flags1;//标识-标志-片偏移

	unsigned short ttl_protocol_checksum0; unsigned short ttl_protocol_checksum1; //ttl-协议-校验

	unsigned short src_ip0; unsigned short src_ip1;//源ip

	unsigned short dst_ip0; unsigned short dst_ip1;//目的ip
};
struct udp_fhdr_hdr
{
	
	unsigned short src_ip0; unsigned short src_ip1;//源ip

	unsigned short dst_ip0; unsigned short dst_ip1;//目的ip
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
		hdr.version_headlen_tos + hdr.length 
		+ hdr.flags0 + hdr.flags1 
		+ hdr.ttl_protocol_checksum0 + hdr.ttl_protocol_checksum1 
		+ hdr.src_ip0 + hdr.src_ip1
		+ hdr.dst_ip0 + hdr.dst_ip1;
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	return (unsigned short)(~checksum);

}
unsigned short cal_udp_checksum(struct udp_fhdr_hdr hdr, unsigned char *buffer)
{
	unsigned long checksum = 0;
	int num = hdr.length-8;
	checksum = 
		hdr.src_ip0 + hdr.src_ip1
		+ hdr.dst_ip0 + hdr.dst_ip1
		+ hdr.reserved+hdr.length
		+ hdr.src_port + hdr.dst_port 
		+ hdr.length + hdr.checksum;
	while (num > 1)
	{
		checksum += (*buffer<<8)+*(buffer+1);
		buffer+=2;
		num -= sizeof(short);
	}
	if (num)
	{
		checksum += *(unsigned short*)buffer << 8;
		
	}
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	return (unsigned short)(~checksum);
}
unsigned short get_checksum(unsigned short *buffer, int size)//size是字节数
{
	unsigned long cksum = 0;
	while (size > 1)
	{
		printf("size=%d  buffer=%x\n", size,*buffer);
		cksum += *buffer;
		buffer++;
		size -= sizeof(short);
		printf("size=%d  checksum=%x\n",size, cksum);
	}
	if (size)
	{
		cksum += *(unsigned short*)buffer<<8;
		printf("size=%d  buffer=%x\n", 1, *buffer);
		printf("size=%d  checksum=%x\n", size, cksum);
	}
	printf("checksum00=%x\n", cksum);
	cksum = (cksum >> 16) + (cksum & 0xffff);
	printf("+ checksum11=%x\n",cksum);
	cksum += (cksum >> 16);
	printf("= checksum22=%x\n", cksum);
	return (unsigned short)(~cksum);
}
int package(struct mac_hdr mhdr, struct ip_hdr ihdr, struct udp_fhdr_hdr uhdr, unsigned char* data, struct rte_mbuf *m)
{
	uint8_t *adcnt=NULL;
	unsigned short ichecksum = cal_ip_checksum(ihdr);
	unsigned short uchecksum = cal_udp_checksum(uhdr,data);
	int total_length=34+uhdr.length;	
	int cnt=0;

	if(total_length>59)
	{	
		cnt=0;				
		//printf("total_length_pa=  %d\n",total_length);				
		rte_pktmbuf_append(m,total_length);			
		for(adcnt=(uint8_t*)m->buf_addr;adcnt<(uint8_t*)rte_pktmbuf_mtod(m,uint8_t*);adcnt++)
			*adcnt=0x00;

		adcnt=rte_pktmbuf_mtod(m,uint8_t*);
		*adcnt=mhdr.dst_mac[0];adcnt++;
		*adcnt=mhdr.dst_mac[1];adcnt++;
		*adcnt=mhdr.dst_mac[2];adcnt++;
		*adcnt=mhdr.dst_mac[3];adcnt++;		
		*adcnt=mhdr.dst_mac[4];adcnt++;
		*adcnt=mhdr.dst_mac[5];adcnt++;

		*adcnt=mhdr.src_mac[0];adcnt++;
		*adcnt=mhdr.src_mac[1];adcnt++;
		*adcnt=mhdr.src_mac[2];adcnt++;
		*adcnt=mhdr.src_mac[3];adcnt++;
		*adcnt=mhdr.src_mac[4];adcnt++;
		*adcnt=mhdr.src_mac[5];adcnt++;


		*adcnt = mhdr.type0; adcnt++;
		*adcnt = mhdr.type1; adcnt++;
		*adcnt = ihdr.version_headlen_tos >> 8; adcnt++;
		*adcnt = ihdr.version_headlen_tos; adcnt++;
		*adcnt = ihdr.length >> 8; adcnt++;
		*adcnt = ihdr.length; adcnt++;
		*adcnt = ihdr.flags0 >> 8; adcnt++;
		*adcnt = ihdr.flags0; adcnt++;
		*adcnt = ihdr.flags1 >> 8; adcnt++;
		*adcnt = ihdr.flags1; adcnt++;
		*adcnt = ihdr.ttl_protocol_checksum0 >> 8; adcnt++;
		*adcnt = ihdr.ttl_protocol_checksum0; adcnt++;
		*adcnt = ichecksum >> 8; adcnt++;
		*adcnt = ichecksum; adcnt++;
		*adcnt = ihdr.src_ip0 >> 8; adcnt++;
		*adcnt = ihdr.src_ip0; adcnt++;
		*adcnt = ihdr.src_ip1 >> 8; adcnt++;
		*adcnt = ihdr.src_ip1; adcnt++;
		*adcnt = ihdr.dst_ip0 >> 8; adcnt++;
		*adcnt = ihdr.dst_ip0; adcnt++;
		*adcnt = ihdr.dst_ip1 >> 8; adcnt++;
		*adcnt = ihdr.dst_ip1; adcnt++;

		*adcnt = uhdr.src_port >> 8; adcnt++;
		*adcnt = uhdr.src_port; adcnt++;			 
		*adcnt = uhdr.dst_port >> 8; adcnt++;
		*adcnt = uhdr.dst_port; adcnt++;
		*adcnt = uhdr.length >> 8; adcnt++;
		*adcnt = uhdr.length; adcnt++;
		*adcnt = uchecksum >> 8; adcnt++;
		*adcnt = uchecksum; adcnt++;

		//printf("uhdrlength = %d\n", uhdr.length);

		for (cnt=0; cnt < uhdr.length - 8; cnt++)
		{
			*adcnt = data[cnt];
			*adcnt++;
		}

		for(adcnt=rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt<(uint8_t*)m->buf_addr+m->buf_len;adcnt++)
			*adcnt=0x00;
		return total_length;
	}
	else
	{
		cnt=0;
		//printf("total_length_pa1=  %d\n",total_length);				
		rte_pktmbuf_append(m,60);
		for(adcnt=(uint8_t*)m->buf_addr;adcnt<(uint8_t*)rte_pktmbuf_mtod(m,uint8_t*);adcnt++)
			*adcnt=0x00;

		adcnt=rte_pktmbuf_mtod(m,uint8_t*);
		*adcnt=mhdr.dst_mac[0];adcnt++;
		*adcnt=mhdr.dst_mac[1];adcnt++;
		*adcnt=mhdr.dst_mac[2];adcnt++;
		*adcnt=mhdr.dst_mac[3];adcnt++;		
		*adcnt=mhdr.dst_mac[4];adcnt++;
		*adcnt=mhdr.dst_mac[5];adcnt++;

		*adcnt=mhdr.src_mac[0];adcnt++;
		*adcnt=mhdr.src_mac[1];adcnt++;
		*adcnt=mhdr.src_mac[2];adcnt++;
		*adcnt=mhdr.src_mac[3];adcnt++;
		*adcnt=mhdr.src_mac[4];adcnt++;
		*adcnt=mhdr.src_mac[5];adcnt++;


		*adcnt = mhdr.type0; adcnt++;
		*adcnt = mhdr.type1; adcnt++;
		*adcnt = ihdr.version_headlen_tos >> 8; adcnt++;
		*adcnt = ihdr.version_headlen_tos; adcnt++;
		*adcnt = ihdr.length >> 8; adcnt++;
		*adcnt = ihdr.length; adcnt++;
		*adcnt = ihdr.flags0 >> 8; adcnt++;
		*adcnt = ihdr.flags0; adcnt++;
		*adcnt = ihdr.flags1 >> 8; adcnt++;
		*adcnt = ihdr.flags1; adcnt++;
		*adcnt = ihdr.ttl_protocol_checksum0 >> 8; adcnt++;
		*adcnt = ihdr.ttl_protocol_checksum0; adcnt++;
		*adcnt = ichecksum >> 8; adcnt++;
		*adcnt = ichecksum; adcnt++;
		*adcnt = ihdr.src_ip0 >> 8; adcnt++;
		*adcnt = ihdr.src_ip0; adcnt++;
		*adcnt = ihdr.src_ip1 >> 8; adcnt++;
		*adcnt = ihdr.src_ip1; adcnt++;
		*adcnt = ihdr.dst_ip0 >> 8; adcnt++;
		*adcnt = ihdr.dst_ip0; adcnt++;
		*adcnt = ihdr.dst_ip1 >> 8; adcnt++;
		*adcnt = ihdr.dst_ip1; adcnt++;

		*adcnt = uhdr.src_port >> 8; adcnt++;
		*adcnt = uhdr.src_port; adcnt++;			 
		*adcnt = uhdr.dst_port >> 8; adcnt++;
		*adcnt = uhdr.dst_port; adcnt++;
		*adcnt = uhdr.length >> 8; adcnt++;
		*adcnt = uhdr.length; adcnt++;
		*adcnt = uchecksum >> 8; adcnt++;
		*adcnt = uchecksum; adcnt++;
		for (cnt=0; cnt < uhdr.length - 8; cnt++)
		{
			*adcnt = data[cnt];
			*adcnt++;
		}

		for(adcnt;adcnt<rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt++)
			*adcnt=0xee;

				
		for(adcnt=rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt<(uint8_t*)m->buf_addr+m->buf_len;adcnt++)
			*adcnt=0x00;
		return total_length;
	}


	
	
}

int read_from_txt(char* a,int num, uint8_t * data)//从文件中将数据读入全局数组b[]中，返回0读取成功，返回1文件不存在
{
	FILE *fp=NULL;
	int i = 0;
	fp = fopen(a, "r");
	if (fp == NULL) { printf("此文件不存在"); return  1; }
	for (i = 0; i < num; i++)
		fscanf(fp, "%x  ", &data[i]);
	//for (i = 0; i < num; i++)
	//	printf("%d   %x\n",i,data[i]);
	fclose(fp);
	return 0;
}

void mmwitfpack_tx(double thput){

	//发送流量
	sendbuf.trthrou = thput;

}

static int l2fwd_main_loop_send(void)
{	
	
	void*d=NULL;
	void**e=&d;
	struct rte_mbuf *m;
	int sent;
	long long int running_second=1;
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
	qconf = &lcore_queue_conf[lcore_id];//此处每一个lcore从全局的conf中获取属于自己的conf
	
	double recieve_rate=0;
	double send_rate=0;
	int mac_length_send=1200;
	int mac_length_recieve=1000;

	RTE_LOG(INFO, L2FWD, "entering main loop send on lcore %u\n", lcore_id);
	for (i = 0; i < qconf->n_rx_port; i++)
	{

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	//int k = 0;
	
	while (!force_quit) 
	{

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if ( unlikely( ( send_en ) && (diff_tsc > drain_tsc) ) )
		{
			portid = 0;
			buffer = tx_buffer[portid];
                                                                
			for(j=0;j<4;j++)                               
			{
				if(rte_ring_mc_dequeue(ring_send,e)<0);						
				else
				{						
					//while(++k < 10) print_mbuf_send(*(struct rte_mbuf **)e);
					sent = rte_eth_tx_buffer(portid, 0, buffer,*(struct rte_mbuf **)e);
					port_statistics[portid].tx += sent;
					rte_pktmbuf_free(*(struct rte_mbuf **)e);
                                e=&d;
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
					running_second+=1;					
					print_stats();
					printf( "%d\n", running_second );
					send_rate=(port_statistics[portid].tx*mac_length_send*8/running_second)/1000000000.0;
					recieve_rate=(port_statistics[portid].rx*mac_length_recieve*8/running_second)/1000000000.0;
					printf( "send_rate= %f Gb\nrecieve_rate= %f Gb\nudp_package_lost= %d\ndata_package_lost= %d\n", 
											send_rate,recieve_rate,udp_package_lost, data_package_lost);

					/* 将发送流量信息显示到界面*/  //mac_length_send
					//将发送流量统计信息发送到界面
					mmwitfpack_tx(send_rate);

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
	//FILE*fp;
	unsigned lcore_id;
	unsigned j, portid, nb_rx;
	int package_recieved=0;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop recieve on lcore  %u\n", lcore_id);
	int k=0;

	while (!force_quit)
	{	
		portid = 0;
		nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst, MAX_PKT_BURST);

		port_statistics[portid].rx += nb_rx;
		//if(package_recieved<400)
		//{
			for (j = 0; j < nb_rx; j++) 
			{
				//MAC包长不是1458,就丢掉
				int pkt_len = pkts_burst[j]->pkt_len;
				int data_len = pkts_burst[j]->data_len;
				if(pkt_len != 1458 || data_len != 1458){
					rte_pktmbuf_free(pkts_burst[j]);
					continue;
				}
				
				//print_mbuf_recieve(pkts_burst[j]);
				while( (!force_quit) && (rte_ring_mp_enqueue(ring_recieve,pkts_burst[j])<0));
				//if(rte_ring_mp_enqueue(ring_recieve,pkts_burst[j])<0)
				//	rte_pktmbuf_free(pkts_burst[j]);		
				rte_pktmbuf_free(pkts_burst[j]);
				package_recieved++;
			}
		//}
		//else break;
	}
}

void gendata(uint8_t *data_tx){
	int len = 106440;
	for(int i = 0; i < len; ++i){
		data_tx[i] = i % 256;
		//data_tx[i] = rand() % 256;
		//data_tx[i] = (i % 250)+1;
	}
}


static void
l2fwd_main_p(void)
{	
	int i, k = 0;
	void*d=NULL;
	void**e=&d;		
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop produce on core %u\n", lcore_id);	
	
	//uint8_t * adcnt=NULL;
	//adcnt = (uint8_t*)malloc(sizeof(uint8_t) * 1400 * PackMaxNumTx);
	int cnt =0;
	int packet_num_threw_in_ring=0;
	long long int packet_num_want_to_send=1;
	int datalength = 1416;	

	int readdatalen= 106440;
	//read_from_txt("data1.txt",readdatalen, adcnt);

	uint16_t pack_cnt=0;	
	
	int total_length=0;
	struct mac_hdr mhdr;
	mhdr.dst_mac[0] = 0xA0;
	mhdr.dst_mac[1] = 0x36;
	mhdr.dst_mac[2] = 0x7A;
	mhdr.dst_mac[3] = 0x58;
	mhdr.dst_mac[4] = 0xAD;
	mhdr.dst_mac[5] = 0x76;

	mhdr.src_mac[0] = 0xA0;
	mhdr.src_mac[1] = 0x36;
	mhdr.src_mac[2] = 0x9F;
	mhdr.src_mac[3] = 0x58;
	mhdr.src_mac[4] = 0xA6;
	mhdr.src_mac[5] = 0x76;
	mhdr.type0 = 0x08;
	mhdr.type1 = 0x00;
	
	struct udp_fhdr_hdr uhdr;
	uhdr.src_port = 0xBEE4;
	uhdr.dst_port = 0xBEE4;
	uhdr.length = datalength+8;
	uhdr.reserved = 0x0011;
	uhdr.src_ip0 = 0xC0A8;
	uhdr.src_ip1 = 0x1405;

	uhdr.dst_ip0 = 0xC0A8;
	uhdr.dst_ip1 = 0x1401;

	struct ip_hdr ihdr;
	ihdr.version_headlen_tos = 0x4500;
	ihdr.length = uhdr.length+20;

	ihdr.flags0 = 0x0000;
	ihdr.flags1 = 0x4000;

	ihdr.ttl_protocol_checksum0 = 0x4011;
	ihdr.ttl_protocol_checksum1 = 0x0000; 

	ihdr.src_ip0 = 0xC0A8;
	ihdr.src_ip1 = 0x1405;
	
	ihdr.dst_ip0 = 0xC0A8;
	ihdr.dst_ip1 = 0x1401;
	
	//data_to_be_sent[0]=0xFF;	
	//data_to_be_sent[1]=0xFF;	
	//data_to_be_sent[2]=0xFF;	
	//data_to_be_sent[3]=0xFF;	
	
	//data_to_be_sent[4]=0x05;	
	//data_to_be_sent[5]=0x77;	
	//for(cnt=6;cnt<1416;cnt++)			
	//{			
	//	data_to_be_sent[cnt]=0x00;
	//}
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	long long int data_cnt_tx = 0;
	long long int running_second = 0;
	timer_tsc = 0;
	prev_tsc = 0;
	double dataSend_rate;


	while (!force_quit)
	{
		//if(rte_ring_mc_dequeue(ring_unpacking,e) < 0); //read data from ring_unpacking 
			//printf("!\n");
		//else{
		
			//adcnt=rte_pktmbuf_mtod(*(struct rte_mbuf **)e,uint8_t*);
			
			//udp package
		
		//printf("%d\t", send_en);
		usleep(10);
		send_en = 0x01;
		if(send_en){
			//如果Bee7发送使能信号给服务器侧

			//send_en = 0x00;

			//随机产生数据
			/*时间统计和速率计算*/
			gendata(data_pack_Tx_forward.data);
					
			pack_cnt = udp_unPacking(data_pack_Tx_forward.data, data_to_be_sent, readdatalen);
			
			//if(k++ < 1){
			//	for(i = 0; i < 1418; ++i){
			//		printf("%02X \n", data_to_be_sent[2][i]);
			//	}
			//}
			//printf("pack_cnt = %d\n", pack_cnt);
			
			//sleep(4);
			for(i = 0; i < pack_cnt; ++i)
			{
				struct rte_mbuf *m;				
				m=rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
				if (m==NULL)
					printf("mempool已满，mbuf申请失败!%d\n",packet_num_threw_in_ring);
				else
				{
			
					//data_to_be_sent[4]=(unsigned char)(packet_num_threw_in_ring>>24);	
					//data_to_be_sent[5]=(unsigned char)(packet_num_threw_in_ring>>16);	
					//data_to_be_sent[6]=(unsigned char)(packet_num_threw_in_ring>>8);	
					//data_to_be_sent[7]=(unsigned char)packet_num_threw_in_ring;	
					//if(pack_cnt==0)
					//{			
					//	data_to_be_sent[i][0]=0x0B;	
					//	data_to_be_sent[i][1]=0x0E;	
					//	data_to_be_sent[i][2]=0x00;	
					//	data_to_be_sent[i][3]=0x00;	
					//	pack_cnt++;
					//}		
					//else if(pack_cnt<384)
					//{
					//	data_to_be_sent[i][0]=0x00;	
					//	data_to_be_sent[i][1]=0x00;	
					//	data_to_be_sent[i][2]=pack_cnt>>8;	
					//	data_to_be_sent[i][3]=pack_cnt;				
					//	pack_cnt++;
					//}
					//else
					//{
					//	data_to_be_sent[i][0]=0x0E;	
					//	data_to_be_sent[i][1]=0x0D;	
					//	data_to_be_sent[i][2]=pack_cnt>>8;	
					//	data_to_be_sent[i][3]=pack_cnt;				
					//	pack_cnt=0;
					//}					
			
					
					total_length = package(mhdr, ihdr, uhdr, data_to_be_sent[i], m);
				    //	printf("total_length=  %d\n",total_length);
					
					while((!force_quit)&&(rte_ring_mp_enqueue(ring_send,m)<0));//printf("p!\n");
					
					packet_num_threw_in_ring++;
					//cnt++;	
				}
			}
			
			//rte_pktmbuf_free(*(struct rte_mbuf **)e);
		//}
		}
	
	}
	printf("入列的包个数%d\n",packet_num_threw_in_ring);
	//free(adcnt);
}


int datacmp(uint8_t *data_tx, uint8_t *data_rx){
	int res = 1;
	int len = 106440;
	for(int i = 0; i < len; ++i){
		if(data_tx[i] != data_rx[i]){
			res = 0;
		}
	}
	return res;
}

void mmwitfpack_rx(uint8_t *conste, uint8_t *channel, double thput){
	
	float a,b;
	//接收流量
	sendbuf.rethrou = thput;
	//printf("rethrou = %f\n",sendbuf.rethrou);
	//星座图
	int copoint = 8192;//13位
	int16_t fix_16;
	int j = 0;
	for(int i = 0; i < 2048; ++i){
		fix_16 = (conste[j]<<8) + conste[j+1];
		sendbuf.constellation[i][0] = (float)fix_16 / copoint;//I
		fix_16 = (conste[j+2]<<8) + conste[j+3];
		sendbuf.constellation[i][1] = (float)fix_16 / copoint;//Q
		j = j + 4;
	}
	//信道1/H
	uint32_t *channel_32 = (uint32_t *) channel;
	int chpoint = 65536;//16位
	int32_t fix_32;
	j = 0;
	for(int i = 0; i < 2048; ++i){
		fix_32 = (channel[j]<<24) + (channel[j+1]<<16) + (channel[j+2]<<8) + channel[j+3];
		a = (float)fix_32 / chpoint;//I
		fix_32 = (channel[j+4]<<24) + (channel[j+5]<<16) + (channel[j+6]<<8) + channel[j+7];
		b = (float)fix_32 / chpoint;//Q
		sendbuf.chan[i][0] = 1/sqrt(a*a+b*b);//幅度
		sendbuf.chan[i][1] = -atan(b/a);//相位
		j = j + 8;
		//printf("%f %f\n",a,b);
	}
	
}

void print_datapack(struct DataPack dataPack){
	FILE*fp;
	fp=fopen("recieve_dataPack","a");
	fprintf(fp,"data_len:%d\n", dataPack.length);
	int adcnt;
	for(adcnt=0;adcnt<dataPack.length;adcnt++)
		fprintf(fp,"%02X ",dataPack.data[adcnt]);
	fprintf(fp,"over\n");
	fclose(fp);
	
}


static void
l2fwd_main_c(void)
{	
	int i;			
	void*d=NULL;
	void**e=&d;	
	//struct rte_eth_dev_tx_buffer *buffer;
	unsigned lcore_id;
	//unsigned portid,j,k;
	int sent;
	uint8_t *adcnt=NULL;

	unsigned short data_length = 0;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop consume on core %u\n", lcore_id);	
	int	packet_num_threw_in_ring_packing = 0;


	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	prev_tsc = 0;
	timer_tsc = 0;

	//FILE *fp=NULL;
	//fp=fopen("luodataint8_t.txt","a");


	//uint16_t pre_pack_hd=0x0E0D;//0B0E包开始 0E0D包结束
	//uint16_t pre_pack_idx=0x0000;//0x0000~0x0181; 包序计数

	uint8_t data_vld;//01包有效  00包无效
	uint16_t pack_idx = -1;
	uint16_t pack_cur;//0x0000~0x0180; 包序计数
	uint16_t data_type = 0x10;
	uint8_t data_type_pre;
 	uint16_t packet_len;//0x0000~0x0578;数据有效长度
	int data_idx = 0; // 已经处理的业务包的个数

	int packet_cnt = 0;  //收到的包的计数
	int packet_cnt_pre = 0;
	int overflow_cnt = 0;
	uint16_t packet_cnt_max = 0xFFFF;
	long long int recieve_data_cnt = 0;
	long long int running_time = 1;
	long long int recieve_data_start = 0;
	double rate_to_display = 0;

	
	int pack_error=0;

	//如果是起始包，设置当前数据为有效数据
	int DataPack_valid = 1;

	int k = 0;
	FILE*fp;

	int flag = 1;
	
	
	while (!force_quit)
	{
		cur_tsc = rte_rdtsc();
		
		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		timer_tsc += diff_tsc;
		if (unlikely(timer_tsc >= timer_period)){
			running_time += 1;
			timer_tsc = 0;
		}
		prev_tsc = cur_tsc;	

		if(rte_ring_mc_dequeue(ring_recieve,e)<0);
			//printf("!\n");
		else
		{	
		
			adcnt=rte_pktmbuf_mtod(*(struct rte_mbuf **)e,uint8_t*);
			
			adcnt+=38;
			data_length=((*adcnt)<<8)+(*(adcnt+1))-8;
			adcnt+=4;
			//for(cnt=0;cnt<data_length;cnt++)				
			//	data_recieved[cnt]=*adcnt++;
			
			//前4个bytes是包头检测序列， 随机生成
			data_vld=*(adcnt + 8);
			//当前包序
			pack_cur=((*(adcnt+9))<<8)+ (*(adcnt+10));
			//printf("pack_cur = %d\n",pack_cur);
			//data_type，数据类型， 2 bytes
			data_type_pre = data_type;
			data_type=*(adcnt+11);
			//data_length，当前数据包的有效数据长度
			packet_len = ((*(adcnt+12))<<8)+ (*(adcnt+13));

			packet_cnt = ((*(adcnt+14))<<8)+ (*(adcnt+15));
			
			if(flag){
				flag = 0;
				recieve_data_start = packet_cnt;
			}
			
			//printf("packet_cnt = %d\n", packet_cnt);
			//printf("overflow_cnt = %d\n", overflow_cnt);
			//printf("recieve_data_cnt = %d\n", recieve_data_cnt);
			if(packet_cnt < packet_cnt_pre){
				overflow_cnt++;
				recieve_data_cnt = overflow_cnt * packet_cnt_max + packet_cnt;
			}
			else{
				recieve_data_cnt = overflow_cnt * packet_cnt_max + packet_cnt;
			}
			packet_cnt_pre = packet_cnt;
			
			if(data_vld)//如果包有效, 一般是接收端BEE7===>服务器
			{
				
		   		if(pack_cur > ++pack_idx){
		   			//将当前数据包清空

		   			pack_error = pack_error + pack_cur - pack_idx; 
		   			pack_idx = pack_cur; 			
		   			//memset(data_pack_Rx[data_idx].data, 0x00, DataMaxLen);
		   		}
		   		else if(pack_cur < pack_idx){//终止包丢失 或者 终止包和起始包同时丢失

		   			udp_package_lost += pack_error + 1; //有可能统计少了，因为不知END位置
		   			data_package_lost++; //上一个数据包丢失
		   			pack_error = pack_cur;
		   			pack_idx = pack_cur;
		   		}		   		
		   		
	   			if(pack_error && pack_cur != 0x0000) {
					rte_pktmbuf_free(*(struct rte_mbuf **)e);
					continue;
				}
				
				
				//++pack_idx;
				printf("========pack_idx = %d\n", pack_idx);

				//printf("%x\t",pack_cur);
		   		if(data_type_pre == 0x10 && data_type == 0x01){
					printf("packet_len = %d\n", packet_len);
		   			printf("data_type = %d\n", data_type);
		   			adcnt += packHeadLen;
		   			pack_idx = 0; //防止终止包丢失的情况
		   			//统计丢包的个数
			   		udp_package_lost += pack_error;
					if(pack_error) {
						data_package_lost++;
						pack_error = 0;
					}
		   			for(i = 0; i < packDataLen; ++i){
		   				data_pack_Rx[data_idx].data[pack_idx * packDataLen + i] = *adcnt++;
		   			}
	
		   		}
		   		else if(data_type == 0x10 && pack_idx == 0x11){   //业务包结束
		   			printf("packet_len = %d\n", packet_len);
		   			printf("data_type = %d\n", data_type);
		   			adcnt += packHeadLen;
		   			for(i = 0; i < packet_len + 1; ++i){
		   				data_pack_Rx[data_idx].data[pack_idx * packDataLen + i] = *adcnt++;
		   			}
		   			
		   			data_pack_Rx[data_idx].length = pack_idx * packDataLen + packet_len + 1;

		   			//业务包结束的时候更新流量
		   			rate_to_display = ((double)(recieve_data_cnt - recieve_data_start) / running_time * 106440.0) / 1000000000.0;
					
		   			//将业务包进行组包，发送并显示
		   			//while((!force_quit)&&(rte_ring_mp_enqueue(ring_packing, &data_pack_Rx[data_idx])<0));
		   			//printf("%x\n", data_type);	
		   			//if(data_type == 0x10){ //如果是信道，读取数据后显示
		   				//printf("data_type");
		   				//将当前包发送，前8196为星座图， 8400-为信道数据信息
						//if(flag) {printf("flag");print_datapack(data_pack_Rx[data_idx]); flag = 0;}
					printf("pack_idx_last = %d\n",pack_idx);
		   			if(packet_len == 0x03D7) mmwitfpack_rx(data_pack_Rx[data_idx].data, data_pack_Rx[data_idx].data + 8400, rate_to_display);
		   			//}				

					packet_num_threw_in_ring_packing++;
		   			
		   			pack_idx = -1;
		   			if(++data_idx == queLen) data_idx = 0;
		   		}
				else{
					//将接收数据放到业务包中
					
					printf("packet_len = %d\n", packet_len);
					printf("data_type = %d\n", data_type);
					adcnt += packHeadLen;
					for(i = 0; i < packDataLen; ++i){
						data_pack_Rx[data_idx].data[pack_idx * packDataLen + i] = *adcnt++;
					}

   				}
				
			}
			else {
				//无效包，发送端BEE7 ===》发送端服务器
				//必须首先进行数据比对， 然后改变send_en状态
				uint8_t senden_reserved = *(adcnt + 9);

				//包序的位置。包的长度发生变化
				pack_cur=((*(adcnt+10))<<8)+ (*(adcnt+11));

				//data_length，当前数据包的有效数据长度
				packet_len = ((*(adcnt+12))<<8)+ (*(adcnt+13));


				//Bee7将数据返回，比对发送数据信息
				if(pack_cur > ++pack_idx){
		   			//将当前数据包清空
		   			DataPack_valid = 0; //无效数据

		   			pack_error = pack_error + pack_cur - pack_idx; 
		   			pack_idx = pack_cur; 			
		   			//memset(data_pack_Rx[data_idx].data, 0x00, DataMaxLen);
		   		}
		   		else if(pack_cur < pack_idx){//终止包丢失 或者 终止包和起始包同时丢失
		   			DataPack_valid = 0; //无效数据

		   			udp_package_lost += pack_error + 1; //有可能统计少了，因为不知END位置
		   			data_package_lost++; //上一个数据包丢失
		   			pack_error = pack_cur;
		   			pack_idx = pack_cur;
		   		}		   		
		   		
	   			if(pack_error && pack_cur != 0x0000) {
					rte_pktmbuf_free(*(struct rte_mbuf **)e);
					continue;
				}


		   		if(pack_cur == 0x0000){
		   			DataPack_valid = 1;  //起始包，包类型为星座图， 初始化为有效数据

		   			adcnt += packHeadLen;
		   			pack_idx = 0; //防止终止包丢失的情况
		   			//统计丢包的个数
			   		udp_package_lost += pack_error;
					if(pack_error) {
						data_package_lost++;
						pack_error = 0;
					}
					//全部空间清零
					memset(data_pack_Tx_backward.data, 0x00, DataMaxLen_Tx);

		   			for(i = 0; i < packDataLen; ++i){
		   				data_pack_Tx_backward.data[pack_idx * packDataLen + i] = *adcnt++;
		   			}
	
		   		}
		   		//业务包结束
		   		else if(pack_cur == 0x004C){
		   			adcnt += packHeadLen;
		   			for(i = 0; i < packet_len; ++i){
		   				data_pack_Tx_backward.data[pack_idx * packDataLen + i] = *adcnt++;
		   			}
		   			
		   			data_pack_Tx_backward.length = pack_idx * packDataLen + packet_len;
	
		   			if(DataPack_valid == 1){  //否则发生丢包情况，不进行比对
		   				//将当前数据包和发送时的数据包进行比对，
		   				//数据包比对函数
		   				datacmp(data_pack_Tx_backward.data, data_pack_Tx_forward.data);
		   				
		   			}				

					packet_num_threw_in_ring_packing++;
		   			
		   			pack_idx = -1;
		   			if(++data_idx == queLen) data_idx = 0;
		   		}
				else{
					//将接收数据放到业务包中
					adcnt += packHeadLen;
					for(i = 0; i < packDataLen; ++i){
						data_pack_Tx_backward.data[pack_idx * packDataLen + i] = *adcnt++;
					}

   				}

   				//使能信号，告知服务器随机产生信号进行发送
				//如果数据无效，利用senden_reserved
				
				send_en = senden_reserved;
				//printf("%d",send_en);

			}


			rte_pktmbuf_free(*(struct rte_mbuf **)e);
			
		}
					

	}
	printf("入列的数据包个数%d\n",packet_num_threw_in_ring_packing);

}

static void
l2fwd_main_showscreen(void){
	int listenfd;
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop showscreen on core %u\n", lcore_id);	

	if ((listenfd = socket(PF_INET, SOCK_STREAM, 0))<0)
	{     //ERR_EXIT("socket");
		perror("socket error");
		return ((void *)1);
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5188);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//servaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	//inet_aton("127.0.0.1",&servaddr.sin_addr);
	int on = 1;
	int length = 34000;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0)
	{     //ERR_EXIT("setsockopt");
		perror("setsockopt error");
		return ((void *)1);
	}
	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0)
	{     //ERR_EXIT("bind");
		perror("bind error");
		return ((void *)1);
	}
	if (listen(listenfd, SOMAXCONN)<0)
	{     //ERR_EXIT("listen");
		perror("listen error");
		return ((void *)1);
	}
	while (!force_quit)
	{
		struct sockaddr_in peeraddr;
		socklen_t peerlen = sizeof(peeraddr);
		int conn;
		if ((conn = accept(listenfd, (struct sockaddr*)&peeraddr, &peerlen))<0)
		{
			perror("listen error");
			break;
		}
		else
			printf("新客户端加入成功 %s:%d\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
		while (!force_quit)
		{
			//str(sendbuf);
			write(conn, &sendbuf, length);
			//printf("cqi0:%d\n",sendbuf.cqi);
			memset(recvbuf, 0, 100);
			int ret = recv(conn, recvbuf, 100, 0);
			//float f=*(float*)(recvbuf+1);
			if (ret>0)
			{
				//for (int i = 0; i < 10; i++)
				//{
				//	printf("%f ", sendbuf.rethrou);
				//}
				//printf("\n");
			}
			else if (ret <= 0)
			{
				printf("客户端中断\n");
				break;
			}
			usleep(100000);
		}
		close(conn);
	}
	close(listenfd);
	return ((void *)0);
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

l2fwd_launch_one_lcore_showscreen(__attribute__((unused)) void *dummy)
{
	l2fwd_main_showscreen();
	return 0;
}


/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

int udp_unPacking(uint8_t *data_in, uint8_t **data_out, int datalen){
	int pack_cnt = 0;
	short pack_idx = 0;
	int i;
	
	//int data_length = data_in[8]<<8 + data_in[9]; //从数据中获取
	int data_length = datalen;
	//如果业务包的长度不够组成一个传输包
	if(data_length < packDataLen){

		//8 bytes随机序列，用于包头检测
		data_to_be_sent[pack_idx][0] = 0xff;
		data_to_be_sent[pack_idx][1] = 0xa0; 
		data_to_be_sent[pack_idx][2] = 0xcb;
		data_to_be_sent[pack_idx][3] = 0x77; 
		data_to_be_sent[pack_idx][4] = 0x6c;
		data_to_be_sent[pack_idx][5] = 0xa0; 
		data_to_be_sent[pack_idx][6] = 0xbf;
		data_to_be_sent[pack_idx][7] = 0x5a;                                   
		//2 bytes 发送包序  //pack_idx
		data_to_be_sent[pack_idx][8] = 0x00;
		data_to_be_sent[pack_idx][9] = 0x00;

		// 1 byte当前数据调制方式
		data_to_be_sent[pack_idx][10] = 0x00;

		//2 bytes 数据包有限数据长度
		data_to_be_sent[pack_idx][11] = data_length>>8;
		data_to_be_sent[pack_idx][12] = data_length;


		for(i = 0; i < data_length; ++i){
			data_to_be_sent[pack_idx][packHeadLen + i] = data_in[i]; 
		}
		pack_cnt = 1;
		return pack_cnt;
	}
	else{
		while(data_length > 0){
			//printf("pack_idx = %d\n",pack_idx);
			if(data_length > packDataLen){
				data_length -= packDataLen;
				//printf("%d\n",data_length);
				//4 bytes 随机序列， 包头检测
				data_to_be_sent[pack_idx][0] = 0xff;
				data_to_be_sent[pack_idx][1] = 0xa0;
				data_to_be_sent[pack_idx][2] = 0xcb;
				data_to_be_sent[pack_idx][3] = 0x77;
				data_to_be_sent[pack_idx][4] = 0x6c;
				data_to_be_sent[pack_idx][5] = 0xa0;
				data_to_be_sent[pack_idx][6] = 0xbf;
				data_to_be_sent[pack_idx][7] = 0x5a;

				//package index
				data_to_be_sent[pack_idx][8] = pack_idx>>8;
				data_to_be_sent[pack_idx][9] = pack_idx;
				//1 byte 调制方式
				data_to_be_sent[pack_idx][10] = 0x00;
				//data length
				data_to_be_sent[pack_idx][11] = packDataLen>>8;
				data_to_be_sent[pack_idx][12] = packDataLen;
				//data
				for(i = 0; i < packDataLen; ++i){
					data_to_be_sent[pack_idx][packHeadLen + i] = data_in[pack_idx * packDataLen + i]; 
				}
				//pack_idx++;
			}
			else{ //终止包
				//printf("%d\n",data_length);
				data_to_be_sent[pack_idx][0] = 0xff;
				data_to_be_sent[pack_idx][1] = 0xa0;
				data_to_be_sent[pack_idx][2] = 0xcb;
				data_to_be_sent[pack_idx][3] = 0x77;
				data_to_be_sent[pack_idx][4] = 0x6c;
				data_to_be_sent[pack_idx][5] = 0xa0;
				data_to_be_sent[pack_idx][6] = 0xbf;
				data_to_be_sent[pack_idx][7] = 0x5a;
				//包序
				data_to_be_sent[pack_idx][8] = pack_idx>>8;
				data_to_be_sent[pack_idx][9] = pack_idx;
				//1 byte 调制方式
				data_to_be_sent[pack_idx][10] = 0x00;
				//2 byte 有效数据长度
				data_to_be_sent[pack_idx][11] = data_length>>8;
				data_to_be_sent[pack_idx][12] = data_length;

				for(i = 0; i < data_length; ++i){
					data_to_be_sent[pack_idx][packHeadLen + i] = data_in[pack_idx * packDataLen + i]; 
				}
				data_length -= packDataLen;
			}
			pack_idx++;
			
		}
		pack_cnt = pack_idx;
	}
	return pack_cnt;
}


int
main(int argc, char **argv)
{
	int ret, i;
	uint8_t nb_ports;
	uint8_t portid;
	unsigned lcore_id;
	
	data_pack_Rx = (struct DataPack *)malloc(sizeof(struct DataPack) * queLen);
	for(i = 0; i < queLen; ++i){
   		data_pack_Rx[i].data = (uint8_t*)calloc(DataMaxLen_Rx, sizeof(uint8_t));
   	}


   	data_pack_Tx_forward.data = (uint8_t*)calloc(DataMaxLen_Tx, sizeof(uint8_t));

   	data_pack_Tx_backward.data = (uint8_t*)calloc(DataMaxLen_Tx, sizeof(uint8_t));

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
	printf( "RTE_MBUF_DEFAULT_BUF_SIZE:%d\n", RTE_MBUF_DEFAULT_BUF_SIZE );
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	printf("mempool init done\n");
	
	ring_send = rte_ring_create("RING_SEND", 1024,rte_socket_id(), 0);
	if(ring_send==NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_send\n");
	printf("ring_send create done\n");


	ring_recieve = rte_ring_create("RING_RECIEVE", 1024,rte_socket_id(), 0);
	if(ring_recieve==NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_recieve\n");
	printf("ring_recieve create done\n");
	
	ring_packing = rte_ring_create("RING_PACKING", 1024,rte_socket_id(), 0);
	if(ring_packing==NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_packing\n");
	printf("ring_packing create done\n");
	
	ring_unpacking = rte_ring_create("RING_UNPACKING", 1024,rte_socket_id(), 0);
	if(ring_unpacking==NULL)
		rte_exit(EXIT_FAILURE, "Cannot init ring_unpacking\n");
	printf("ring_unpacking create done\n");

	//check ethernet ports 	
	nb_ports=rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
	else
		printf("number of Ethernet ports that are available:%d\n",rte_eth_dev_count());


	/* Initialise each port */
	portid=0;
	/* init port */
	printf("Initializing port %u... ", (unsigned) portid);
	fflush(stdout);
	ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",ret, (unsigned) portid);

	rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

	/* init one RX queue */
	fflush(stdout);
	ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
				     rte_eth_dev_socket_id(portid),
				     NULL,
				     l2fwd_pktmbuf_pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
			  ret, (unsigned) portid);

	/* init one TX queue on each port */
	fflush(stdout);
	ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
			rte_eth_dev_socket_id(portid),
			NULL);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
			ret, (unsigned) portid);

	/* Initialize TX buffers */
	tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
			rte_eth_dev_socket_id(portid));
	if (tx_buffer[portid] == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
				(unsigned) portid);

	rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
			rte_eth_tx_buffer_count_callback,
			&port_statistics[portid].dropped);
	if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot set error callback for "
					"tx buffer on port %u\n", (unsigned) portid);

	/* Start device */
	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
			  ret, (unsigned) portid);

	printf("done: \n");

	rte_eth_promiscuous_enable(portid);

	printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
			(unsigned) portid,
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
	sleep(5);	
	rte_eal_remote_launch(l2fwd_launch_one_lcore_showscreen, NULL, 3);	
	sleep(6);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_p, NULL, 4);	
	sleep(3);
	rte_eal_remote_launch(l2fwd_launch_one_lcore_send, NULL, 5);
		
	
	
	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		if (rte_eal_wait_lcore(lcore_id) < 0)
		{
			ret = -1;
			break;
		}
	}

/*
	FILE *fp;
	FILE *fpp;
	struct rte_mbuf *m;				
	m=rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
	
	uint8_t * adcnt;
	
	for(adcnt=(uint8_t*)m->buf_addr;adcnt<(uint8_t*)rte_pktmbuf_mtod(m,uint8_t*);adcnt++)
		*adcnt=0xaa;
	for(adcnt=rte_pktmbuf_mtod(m,uint8_t*);adcnt<rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt++)
		*adcnt=0xaa;
	for(adcnt=rte_pktmbuf_mtod(m,uint8_t*)+m->data_len;adcnt<(uint8_t*)m->buf_addr+m->buf_len;adcnt++)
		*adcnt=0xaa;
	
	fp=fopen("status","w");
	struct rte_ring *r = rte_ring_create("MY_RING", 1024,rte_socket_id(), 0);
	rte_ring_dump (fp, r);
	rte_ring_mp_enqueue(r,m);
	rte_ring_dump (fp, r);
	rte_ring_mc_dequeue(r,e);
	rte_ring_dump (fp, r);
	fclose(fp);
	print_mbuf_send(*(struct rte_mbuf **)e,fpp);
	rte_pktmbuf_free(*(struct rte_mbuf **)e);
	print_mbuf_send(*(struct rte_mbuf **)e,fpp);
*/
	portid = 0;
		
	printf("Closing port %d...", portid);
	rte_eth_dev_stop(portid);
	rte_eth_dev_close(portid);
	printf(" Done\n");

	printf("Bye...\n");
	print_stats();

	for(i = 0; i < queLen; ++i){
   		free(data_pack_Rx[i].data);
   	}
	free(data_pack_Tx_forward.data);
	free(data_pack_Tx_forward.data);

	return ret;
}



//export RTE_SDK=$HOME/dpdk-stable-17.02.1
//export RTE_TARGET=build
//make
//su
//./build/l2fwd -c ff -n 2
//
//sudo modprobe uio_pci_generic
//./dpdk-devbind.py -b uio_pci_generic 81:00.0


//./dpdk-devbind.py -s


