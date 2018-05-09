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

static volatile bool force_quit;


#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

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


unsigned char data_to_be_sent[2048]={0};
unsigned char packet_to_be_sent[2048]={0};
unsigned char data_recieved[2048]={0};
uint8_t send_en=0x00;//此变量后面改为全局变量 00停止发送  01开始发送
int pack_err=0;
int else1=0;
int else2=0;

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

int read_from_txt(char* a,int num)//从文件中将数据读入全局数组b[]中，返回0读取成功，返回1文件不存在
{
	FILE *fp=NULL;
	int i = 0;
	fp = fopen(a, "r");
	if (fp == NULL) { printf("此文件不存在"); return  1; }
	for (i = 0; i < num;i++)
		fscanf(fp, "%x  ", &data_to_be_sent[i]);
	for (i = 0; i < num; i++)
		printf("%d   %x\n",i,data_to_be_sent[i]);
	fclose(fp);
	return 0;
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
	int mac_length_send=1458;
	int mac_length_recieve=1458;

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
		if ( unlikely( ( send_en ) && (diff_tsc > drain_tsc) ) )
		{
			portid = 0;
			buffer = tx_buffer[portid];
                                                                
			for(j=0;j<4;j++)                               
			{
				if(rte_ring_mc_dequeue(ring_send,e)<0);						
				else
				{						
					//print_mbuf_send(*(struct rte_mbuf **)e);
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
					printf( "send_rate= %f Gb\nrecieve_rate= %f Gb\n", send_rate,recieve_rate );
					printf( "pack_err= %d\n", pack_err );	
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
	FILE*fp;
	unsigned lcore_id;
	unsigned j, portid, nb_rx;
	int package_recieved=0;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop recieve on lcore  %u\n", lcore_id);

	while (!force_quit)
	{	
		portid = 0;
		nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst, MAX_PKT_BURST);

		port_statistics[portid].rx += nb_rx;
		if(package_recieved<400)
		{
			for (j = 0; j < nb_rx; j++) 
			{
				print_mbuf_recieve(pkts_burst[j]);
				rte_ring_mp_enqueue(ring_recieve,pkts_burst[j]);		
				//rte_pktmbuf_free(pkts_burst[j]);
				package_recieved++;
			}
		}
		else break;
	}
}



static void
l2fwd_main_p(void)
{				
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop produce on core %u\n", lcore_id);	
	
	uint8_t * adcnt=NULL;
	int cnt =0;
	int packet_num_threw_in_ring=0;
	long long int packet_num_want_to_send=1;
	int datalength=1416;
	//read_from_txt("data.txt",datalength);
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
	
	data_to_be_sent[0]=0xFF;	
	data_to_be_sent[1]=0xFF;	
	data_to_be_sent[2]=0xFF;	
	data_to_be_sent[3]=0xFF;	
	
	data_to_be_sent[4]=0x05;	
	data_to_be_sent[5]=0x77;	
	for(cnt=6;cnt<1416;cnt++)			
	{			
		data_to_be_sent[cnt]=0x00;
	}

	


	while (!force_quit)
	{
		
		//if(packet_num_threw_in_ring>=packet_num_want_to_send)break;		
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
			if(pack_cnt==0)
			{			
				data_to_be_sent[0]=0x0B;	
				data_to_be_sent[1]=0x0E;	
				data_to_be_sent[2]=0x00;	
				data_to_be_sent[3]=0x00;	
				pack_cnt+=190;
			}
			else if(pack_cnt<385)
			{
				data_to_be_sent[0]=0x00;	
				data_to_be_sent[1]=0x00;	
				data_to_be_sent[2]=pack_cnt>>8;	
				data_to_be_sent[3]=pack_cnt;				
				pack_cnt++;
			}
			else
			{
				data_to_be_sent[0]=0x00;	
				data_to_be_sent[1]=0x00;	
				data_to_be_sent[2]=pack_cnt>>8;	
				data_to_be_sent[3]=pack_cnt;				
				pack_cnt=0;
			}
			
			
			
			
			total_length = package(mhdr, ihdr, uhdr, data_to_be_sent, m);
		//	printf("total_length=  %d\n",total_length);

			while((!force_quit)&&(rte_ring_mp_enqueue(ring_send,m)<0));//printf("p!\n");
				
			packet_num_threw_in_ring++;
			//cnt++;	
		}
			
	}
	printf("入列的包个数%d\n",packet_num_threw_in_ring);
	
}
static void
l2fwd_main_c(void)
{				
	void*d=NULL;
	void**e=&d;	
	struct rte_eth_dev_tx_buffer *buffer;
	unsigned lcore_id;
	unsigned portid,j,k;
	int sent;
	uint8_t *adcnt=NULL;
	int cnt=0;
	unsigned short data_length=0;
	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, L2FWD, "entering main loop consume on core %u\n", lcore_id);	
	
	FILE *fp=NULL;
	fp=fopen("luodataint8_t.txt","a");


	uint16_t pre_pack_hd=0x0E0D;//0B0E包开始 0E0D包结束
	uint16_t pre_pack_idx=0x0000;//0x0000~0x0181; 包序计数

	uint8_t data_vld;//01包有效  00包无效
	uint16_t pack_hd;//0B0E包开始 0E0D包结束
	//uint8_t send_en;//此变量后面改为全局变量 00停止发送  01开始发送
	uint16_t pack_idx;//0x0000~0x0181; 包序计数
	uint16_t packet_len;//0x0000~0x0577;数据有效长度
	
	uint8_t new_group=0x00;//01 新的包来了而且还没结束 00正在等待新的包
	//int pack_err=0;
	//int else1=0;
	//int else2=0;





	while (!force_quit)
	{
		if(rte_ring_mc_dequeue(ring_recieve,e)<0);
			//printf("!\n");
		else
		{
			cnt=0;
			
			adcnt=rte_pktmbuf_mtod(*(struct rte_mbuf **)e,uint8_t*);
			adcnt+=38;
			data_length=((*adcnt)<<8)+(*(adcnt+1))-8;
			adcnt+=4;
			//for(cnt=0;cnt<data_length;cnt++)				
			//	data_recieved[cnt]=*adcnt++;
			
			data_vld=*(adcnt);
			//pack_hd=*(adcnt+1);
			pack_hd=((*(adcnt+1))<<8)+ (*(adcnt+2));
			//pack_idx=*(adcnt+3);
			pack_idx=((*(adcnt+3))<<8)+ (*(adcnt+4));
			//packet_len=*(adcnt+5);
			packet_len=((*(adcnt+5))<<8)+ (*(adcnt+6));
			send_en=*(adcnt+7);
			
			if(data_vld)//如果包有效
			{
				if( (pack_hd==0x0B0E) && (pack_idx==0x0000) )//新的一组包来了
				{
					pack_err+=pack_idx+0x0180-pre_pack_idx;						
					pre_pack_hd=0x0B0E;
					pre_pack_idx=0x0000;
					new_group=0x01;					
				}
				else if( (new_group==0x01) && (pack_hd==0x0000)  )//新的一组包的中间的包
				{
					pack_err+=pack_idx-pre_pack_idx;
					pre_pack_hd=pack_hd;
					pre_pack_idx=pack_idx;
					new_group=0x01;					

				}
				else if( (pack_hd==0x0E0D) && (pack_idx==0x0180) )//新的一组包的最后一个包
				{
					pack_err+=pack_idx-pre_pack_idx;					
					pre_pack_hd=0x0E0D;
					pre_pack_idx=0x0180;
					new_group=0x00;				
				}
				
				else//出错了
				{
					else1++;
					//printf("%02X\n",data_vld);
					//printf("%04X\n",pack_hd);
					//printf("%04X\n",pack_idx);
					//printf("%04X\n",packet_len);
					//printf("%02X\n",send_en);				
				}

				/*{
					pre_pack_hd=0x0E0D;
					pre_pack_idx=0xFFFF;
					new_group=0x00;
					pack_err++;					
				}*/
			}
			else//无效包
				else2++;



			
			
			/*
			for(cnt=0;cnt<data_length;cnt+=sizeof(int8_t))
			{					
				fprintf(fp,"%d\n", *(int8_t*)adcnt);
				adcnt+=sizeof(int8_t);		
			}
			*/
			rte_pktmbuf_free(*(struct rte_mbuf **)e);
			int p=0;
			//for(p=0;p<data_length;p++)printf("%d %x\n",p,data_recieved[p]);
			

		}
					

	}
	fclose(fp);
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

int
main(int argc, char **argv)
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
	printf("else1=%d,else2=%d\n",else1,else2);
	return ret;
}

