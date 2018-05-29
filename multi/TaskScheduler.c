#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <math.h>
#include <sys/time.h>
#include "mkl.h"
// #include "omp.h"
#include "thread_pool.h"
#include "srslte/fec/crc.h"
#include "srslte/fec/turbocoder.h"
#include "srslte/fec/turbodecoder.h"
#include "srslte/fec/rm_turbo.h"
#include "crc_cbsegm.h"
#include "crc_mod.h"
#include "chest_calsym.h"
#include "derm_crc.h"
#include "crc_check.h"
#include "config.h"
#define LAYER_NUM 8
#define CQI_INDEX 15
int *ServiceEN_tx;
int *ServiceEN_rx;
extern sem_t tx_prepared;
extern sem_t rx_prepared;
extern sem_t tx_can_be_destroyed;
extern sem_t rx_can_be_destroyed;
extern sem_t tx_buff_prepared;
extern sem_t rx_buff_prepared;
extern sem_t cache_tx;
extern sem_t cache_rx;
#define PARA_NUM_TX 2 // 发送端同时处理子帧上限
#define PARA_NUM_RX 5 // 接收端同时处理子帧上限
const int SBNum = 50; // 信道估计相关
//test
struct package_t package_tx[PACK_CACHE];
struct package_t package_rx[PACK_CACHE];
int index_tx_write; // 发送段缓存写指针
int index_rx_read;  // 接收端缓存读指针
int index_tx_read;
int index_rx_write;
lapack_complex_float *H[1];
const int testError = 100;
const int testTime = 100;
struct RunTime runtime;
int readyNum_tx;
int readyNum_rx;
int startNum_tx;
int startNum_rx;
pthread_mutex_t mutex_startNum_tx;
pthread_mutex_t mutex_startNum_rx;
pthread_mutex_t mutex_readyNum_tx;
pthread_mutex_t mutex_readyNum_rx;
int runIndex = 0;
int buffisEmpty;

const int CQI_mod[16] = {0, 2, 2, 2, 2, 2, 2, 4, 4, 4, 6, 6, 6, 6, 6, 6};
const int CQI_cod[16] = {0, 78, 120, 193, 308, 449, 602, 378, 490, 616, 466, 567, 666, 772, 873, 948};
const float CQI_mapping_table_LS[15] = {3, 4, 5, 7, 8, 9, 13, 14, 16, 17, 19, 20, 22, 24, 27};
const float CQI_mapping_table_DCT[15] = {-4, -2, -1, 1, 3, 5, 8, 9, 11, 14, 16, 17, 19, 21, 24};
const int datasymbNum[10] = {12408, 10728, 12540, 13200, 13200, 13128, 13128, 13200, 13200, 13200};

void TaskScheduler_tx(void *arg)
{

	//--------------------Set the initial parameters--------------------
	readyNum_tx = 0, startNum_tx = 0;
	pthread_mutex_init(&mutex_startNum_tx, NULL);
	pthread_mutex_init(&mutex_readyNum_tx, NULL);
	for (int index_tx_write = 0; index_tx_write < PACK_CACHE; index_tx_write++)
	{
		package_tx[index_tx_write].tbs = (int *)malloc(sizeof(int) * MAX_BEAM);
		package_tx[index_tx_write].CQI_index = (int *)malloc(sizeof(int) * MAX_BEAM);
	}
	uint32_t seed = 1;
	int k = 0, t = 0;
	//seed = time(NULL);
	srand(seed);
	int LayerNum = LAYER_NUM;
	int *CQI_index = (int *)malloc(sizeof(int) * MAX_BEAM * PACK_CACHE);
	for (int i = 0; i < PACK_CACHE; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			CQI_index[i * MAX_BEAM + j] = CQI_INDEX;
		}
	}
	int inter_freq = LayerNum;
	while (CARRIER_NUM / SBNum % inter_freq != 0)
		inter_freq++;

	float SNR_MIN = 15, SNR_MAX = 15, STEP = 1.0;
	float SNR = SNR_MIN - STEP;
	float sigma = 0;

	//-----time test-----
	struct timeval tx_begin, tx_end;
	runtime.tx = 0;
	int bitsNum = 0;
	int TIME_EN = 0;
	//  1.0  Generate data
	int data_len_tx[PARA_NUM_TX][MAX_BEAM];
	uint8_t *data_tx[PARA_NUM_TX][MAX_BEAM];
	uint8_t *data_bytes_tx[PARA_NUM_TX][MAX_BEAM];
	for (int i = 0; i < PARA_NUM_TX; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (MAX_DATA_LEN_TX + CRC_LENGTH));
			data_bytes_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (MAX_DATA_LEN_TX + CRC_LENGTH) / 8);
		}
	}
	for (int index_tx_write = 0; index_tx_write < PACK_CACHE; index_tx_write++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			package_tx[index_tx_write].data[j] = (uint8_t *)malloc(sizeof(uint8_t) * MAX_DATA_LEN_TX + CRC_LENGTH);
		}
	}
	//--------------------The preparation of the TX--------------------

	//  1.1  crc attach - code block segmentation
	struct crc_cbsegm_args_t *crc_cbsegm_args = (struct crc_cbsegm_args_t *)malloc(sizeof(struct crc_cbsegm_args_t) * PARA_NUM_TX * MAX_BEAM);
	srslte_crc_t *crc_tb[PARA_NUM_TX][MAX_BEAM];
	srslte_cbsegm_t *cb_tx[PARA_NUM_TX][MAX_BEAM];
	for (int i = 0; i < PARA_NUM_TX; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			crc_tb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_tb[i][j], CRC_24A, CRC_LENGTH);
			cb_tx[i][j] = (srslte_cbsegm_t *)malloc(sizeof(srslte_cbsegm_t));
		}
	}
	printf("TX task1 initialized \n");
	//  1.2  crc attach - turbo coding - rate matching - code block concatenation
	const int cbNum = (MAX_DATA_LEN_TX + CRC_LENGTH) / 6144 + 1;
	srslte_crc_t *crc_cb[PARA_NUM_TX][MAX_BEAM * cbNum];
	for (int i = 0; i < PARA_NUM_TX; i++)
	{
		for (int j = 0; j < MAX_BEAM * cbNum; j++)
		{
			crc_cb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_cb[i][j], CRC_24B, CRC_LENGTH);
		}
	}
	struct crc_mod_args_t *crc_mod_args = (struct crc_mod_args_t *)malloc(sizeof(struct crc_mod_args_t) * PARA_NUM_TX * MAX_BEAM * cbNum);
	uint8_t *cr_data_bytes_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	srslte_tcod_t *tcod[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint8_t *cr_tcod_data_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint8_t *cr_rm_data_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint8_t *cr_rm_data_bytes_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint8_t *w_buff_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint8_t *parity_bytes[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint8_t *rm_data_tx[PARA_NUM_TX][MAX_BEAM];
	uint32_t rm_data_len_tx[PARA_NUM_TX][MAX_BEAM];
	uint32_t rm_outlen_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	uint32_t Lamda;
	int cr_symnum_tx[PARA_NUM_TX][MAX_BEAM * cbNum];
	int SymbolBitN[PARA_NUM_TX][MAX_BEAM];
	lapack_complex_float *modsymb[PARA_NUM_TX];
	for (int i = 0; i < PARA_NUM_TX; i++)
	{
		for (int j = 0; j < MAX_BEAM * cbNum; j++)
		{
			cr_data_bytes_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB / 8);
			tcod[i][j] = (srslte_tcod_t *)malloc(sizeof(srslte_tcod_t));
			srslte_tcod_init(tcod[i][j], SRSLTE_TCOD_MAX_LEN_CB);
			cr_tcod_data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * 6176 * 3);
			cr_rm_data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * MAX_RM_DATA_LEN_TX);
			cr_rm_data_bytes_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * MAX_RM_DATA_LEN_TX / 8 + 1);
			w_buff_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * 6176 * 3);
			parity_bytes[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (6148 * 2 / 8 + 1));
		}
		modsymb[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM);
	}
	for (int i = 0; i < PARA_NUM_TX; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			rm_data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * MAX_RM_DATA_LEN_TX);
		}
	}

	int *detc_cb_sizes = (int *)malloc(sizeof(int) * 6145);
	for (int i = 1; i <= 6144; i++)
	{
		detc_cb_sizes[i] = 0;
	}
	for (int i = 0; i < 188; i++)
	{
		detc_cb_sizes[srslte_cbsegm_cbsize(i)] = i;
	}
	srslte_tcod_gentable();
	srslte_rm_turbo_gentables();

	printf("TX task2 initialized \n");

	//  1.3  packing
	int cbtaskNum[PARA_NUM_TX];
	lapack_complex_float *x[PACK_CACHE];
	for (int i = 0; i < PACK_CACHE; i++)
	{
		x[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM);
	}
	index_tx_write = 0;
	lapack_complex_float *PilotSymb[MAX_BEAM];
	for (int i = 0; i < MAX_BEAM; i++)
	{
		PilotSymb[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * (i + 1) * CARRIER_NUM * PILOT_SYM_NUM);
		CalPilotSymb(i + 1, CARRIER_NUM, 2, 1, inter_freq, PilotSymb[i]);
	}

	//--------------------Loading channel information--------------------
	//Channel matrix
	uint32_t m = 0;
	const int channelNum = 1;
	//int H_layer[8] = {91,90,93,92,89,88,87,95};//1
	//int H_layer[8] = {87,88,89,90,91,92,93};

	int H_layer[8] = {76, 77, 75, 73, 74, 72, 78, 79}; //2
	//int H_layer[8] = {72,73,74,75,76,77,78,79};

	//int H_layer[8] = {66,67,68,65,69,64,70,63};//3
	//int H_layer[8] = {63,64,65,66,67,68,69,70};

	lapack_complex_float *channel = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * TX_ANT_NUM * CARRIER_NUM * SYMBOL_NUM * channelNum);
	FILE *fp_hr;
	fp_hr = fopen("channel/H_real2.txt", "r");
	if (!fp_hr)
		printf("Can not open the file.\n");
	while (!feof(fp_hr))
	{
		fscanf(fp_hr, "%f", &(channel[m].real));
		//if(m < 10) printf("%d %e\n",m,channel[m].real);
		m++;
	}
	fclose(fp_hr);
	//printf("m = %d\n",m);

	FILE *fp_hi;
	fp_hi = fopen("channel/H_imag2.txt", "r");
	m = 0;
	if (!fp_hi)
		printf("Can not open the file.\n");
	while (!feof(fp_hi))
	{
		fscanf(fp_hi, "%f", &(channel[m].imag));
		//if(m < 10) printf("%d %e\n",m,channel[m].imag);
		m++;
	}
	fclose(fp_hi);
	//printf("m = %d\n",m);

	for (int t = 0; t < channelNum; t++)
	{
		H[t] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM);
	}
	for (int t = 0; t < channelNum; t++)
	{
		for (int ns = 0; ns < SYMBOL_NUM; ns++)
		{
			for (int nc = 0; nc < CARRIER_NUM; nc++)
			{
				for (int nl = 0; nl < LayerNum; nl++)
				{
					for (int nr = 0; nr < RX_ANT_NUM; nr++)
					{
						H[t][ns * CARRIER_NUM * LayerNum * RX_ANT_NUM + nc * LayerNum * RX_ANT_NUM + nl * RX_ANT_NUM + nr] = channel[t * RX_ANT_NUM * TX_ANT_NUM * CARRIER_NUM * SYMBOL_NUM + ns * CARRIER_NUM * TX_ANT_NUM * RX_ANT_NUM + nc * TX_ANT_NUM * RX_ANT_NUM + H_layer[nl] * RX_ANT_NUM + nr];
					}
				}
			}
		}
	}
	free(channel);

	//AWGN
	FILE *fp_nr;
	fp_nr = fopen("channel/wgnr.txt", "r");
	FILE *fp_ni;
	fp_ni = fopen("channel/wgni.txt", "r");
	lapack_complex_float *gwn = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * CARRIER_NUM * SYMBOL_NUM);
	m = 0;
	while (!feof(fp_nr))
	{
		fscanf(fp_nr, "%f", &(gwn[m].real));
		m++;
	}
	m = 0;
	while (!feof(fp_ni))
	{
		fscanf(fp_ni, "%f", &(gwn[m].imag));
		m++;
	}
	//for(int i =0; i < 10; i++) printf("%f+%fi\n",gwn[i].real,gwn[i].imag);
	fclose(fp_nr);
	fclose(fp_ni);
	printf("Channel loading is finished...\n");

	//Through the channel
	lapack_complex_float *H_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * MAX_BEAM);
	lapack_complex_float *x_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MAX_BEAM * 1);
	lapack_complex_float *y_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * 1);
	lapack_complex_float *y[PACK_CACHE];
	for (int i = 0; i < PACK_CACHE; i++)
	{
		y[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * CARRIER_NUM * SYMBOL_NUM);
	}

	const CBLAS_LAYOUT Layout = CblasColMajor;
	const CBLAS_TRANSPOSE transa = CblasNoTrans;
	const CBLAS_TRANSPOSE transb = CblasNoTrans;
	const MKL_INT _m = RX_ANT_NUM;
	const MKL_INT _n = 1;
	const MKL_INT _k = LayerNum;
	lapack_complex_float alpha;
	alpha.real = 1;
	alpha.imag = 0;
	lapack_complex_float beta;
	beta.real = 0;
	beta.imag = 0;
	const MKL_INT lda = _m;
	const MKL_INT ldb = _k;
	const MKL_INT ldc = _m;

	if (TIME_EN == 1)
	{
		//  1.0  generate data
		for (k = 0; k < PARA_NUM_TX; k++)
		{
			srand(seed);
			for (int i = 0; i < LayerNum; i++)
			{
				data_len_tx[k][i] = CQI_cod[CQI_index[k * MAX_BEAM + i]] / 1024.0 * MAX_SYMBOL_NUM * CQI_mod[CQI_index[k * MAX_BEAM + i]] - CRC_LENGTH;
				//data_len_tx[k][i] = (rand() % (data_len_tx[k][i] / 8) + 1) * 8;
				//data_len_tx[k][i] = (rand() % 10 + 1) * 10;
				//printf("\nLayer %d : %d Original bits : ", i, data_len_tx[k][i]);
				for (int j = 0; j < data_len_tx[k][i]; j++)
				{
					// data_tx[k][i][j] = rand() % 2;
					data_tx[k][i][j] = 1;
					//printf("%d",data_tx[k][i][j]);
				}
			}
		}
	}
	else
		TIME_EN = 0;

	int count_ms = 0, count_s = 0, TIME = 0;

	printf("TaskScheduler tx prepared...\n");
	sem_post(&tx_prepared);
	sem_post(&tx_prepared);
	sem_wait(&rx_prepared);
	sem_wait(&tx_buff_prepared);
	//--------------------Data processing--------------------
	ServiceEN_tx = (int *)malloc(sizeof(int) * PARA_NUM_TX * TASK_NUM_TX);
	for (int i = 0; i < PARA_NUM_TX; i++)
	{
		for (int j = 0; j < TASK_NUM_TX; j++)
		{
			ServiceEN_tx[i * TASK_NUM_TX + j] = 0;
		}
	}
	for (int i = 0; i < PARA_NUM_TX; i++)
		ServiceEN_tx[TASK_NUM_TX * i] = 1;
	if (TIME_EN == 1)
		gettimeofday(&tx_begin, NULL); //--------------------rx

	while (1)
	{
		//sleep(1);
		//if(runIndex >= 3000) break;
		for (int n = 0; n < PARA_NUM_TX; n++)
		{
			//  1.1  crc attach - code block segmentation
			if (startNum_tx < PACK_CACHE && ServiceEN_tx[n * TASK_NUM_TX] == 1)
			{ //有数据需要发送，且任务添加器(n,1)打开
				pthread_mutex_lock(&mutex_startNum_tx);
				startNum_tx++;
				pthread_mutex_unlock(&mutex_startNum_tx);
				if (TIME_EN == 0)
				{
					// get new original data
					for (int i = 0; i < LayerNum; i++)
					{
						//CQI_index[n * MAX_BEAM + i] = rand()%15+1;
						data_len_tx[n][i] = (int)(CQI_cod[CQI_index[i]] / 1024.0 * MAX_SYMBOL_NUM * CQI_mod[CQI_index[i]] - CRC_LENGTH) / 8 * 8;
						//data_len_tx[n][i] = (rand() % (data_len_tx[n][i] / 8) + 1) * 8;
						//data_len_tx[k][i] = (rand() % 10 + 1) * 10;
						//printf("\nLayer %d : %d Original bits : ", i, data_len_tx[k][i]);
						for (int j = 0; j < data_len_tx[n][i] / 8; j++)
						{
							// data_bytes_tx[n][i][j] = rand() % 256;
							data_bytes_tx[n][i][j] = 0xFF;
							//printf("%d",data_tx[k][i][j]);
						}
						srslte_bit_unpack_vector(data_bytes_tx[n][i], data_tx[n][i], data_len_tx[n][i]);
					}
				}

				for (int i = 0; i < LayerNum; i++)
				{
					int j = n * MAX_BEAM + i;
					crc_cbsegm_args[j].crc_p = crc_tb[n][i];
					crc_cbsegm_args[j].crc_poly = CRC_24A;
					crc_cbsegm_args[j].crc_length = CRC_LENGTH;
					crc_cbsegm_args[j].tbs = data_len_tx[n][i];
					crc_cbsegm_args[j].tb = data_bytes_tx[n][i];
					crc_cbsegm_args[j].cb_tx = cb_tx[n][i];
					// printf("j=%d,->C=%d\n", j, crc_cbsegm_args[j].cb_tx);

					crc_cbsegm_args[j].ServiceEN = ServiceEN_tx;
					crc_cbsegm_args[j].ServiceEN_index = n * TASK_NUM_TX + 1;

					pool_add_task(crc_cbsegm, (void *)&crc_cbsegm_args[j], 2);
				}
				ServiceEN_tx[n * TASK_NUM_TX] = 0; //关闭任务添加器(n,1)
			}

			//  1.2  crc attach - turbo coding - rate matching - code block concatenation
			if (ServiceEN_tx[n * TASK_NUM_TX + 1] == LayerNum)
			{ //任务添加器(n,2)打开
				// for (int i = 0; i < 2; i++)
				// 	for (int j = 0; j < 8; j++)
				// 	{
				// 		printf("C[%d][%d]=%d\n", i, j, cb_tx[i][j]->C);
				// 		printf("arg->C[%d][%d]=%d\n", i, j, crc_cbsegm_args[n * 8 + j].cb_tx->C);
				// 	}
				// for (int i = 0; i < 6; i++)
				// 	printf("ServiceEN_tx[%d]=%d\n", i, ServiceEN_tx[i]);
				// exit(0);

				int s = 0;
				cbtaskNum[n] = 0;
				for (int i = 0; i < LayerNum; i++)
				{
					cbtaskNum[n] += cb_tx[n][i]->C;
					rm_data_len_tx[n][i] = MAX_SYMBOL_NUM * CQI_mod[CQI_index[n * MAX_BEAM + i]];
					Lamda = MAX_SYMBOL_NUM % cb_tx[n][i]->C;
					// printf("C:%d\n",cb_tx[n][i]->C);
					for (int r = 0; r < (cb_tx[n][i]->C); ++r)
					{
						int j = i * cbNum + r;
						int jp = n * LayerNum * cbNum + i * cbNum + r;
						if (r < cb_tx[n][i]->C - Lamda)
							rm_outlen_tx[n][j] = CQI_mod[CQI_index[n * MAX_BEAM + i]] * (MAX_SYMBOL_NUM / cb_tx[n][i]->C);
						else
							rm_outlen_tx[n][j] = CQI_mod[CQI_index[n * MAX_BEAM + i]] * ((MAX_SYMBOL_NUM - 1) / cb_tx[n][i]->C + 1);
						SymbolBitN[n][i] = CQI_mod[CQI_index[n * MAX_BEAM + i]];
						cr_symnum_tx[n][j] = rm_outlen_tx[n][j] / SymbolBitN[n][i];

						crc_mod_args[jp].cbindex = r;
						crc_mod_args[jp].cb_tx = cb_tx[n][i];
						crc_mod_args[jp].tbp = data_bytes_tx[n][i];

						crc_mod_args[jp].crc_p = crc_cb[n][j];
						crc_mod_args[jp].crc_poly = CRC_24B;
						crc_mod_args[jp].crc_length = CRC_LENGTH;
						crc_mod_args[jp].cb = cr_data_bytes_tx[n][j];

						crc_mod_args[jp].tcod = tcod[n][j];
						crc_mod_args[jp].cb_tcod = cr_tcod_data_tx[n][j];

						crc_mod_args[jp].cbs_rm = rm_outlen_tx[n][j];
						crc_mod_args[jp].cb_rm = cr_rm_data_tx[n][j];
						crc_mod_args[jp].cb_rm_bytes = cr_rm_data_bytes_tx[n][j];
						crc_mod_args[jp].w_buff = w_buff_tx[n][j];
						crc_mod_args[jp].rv_idx = 0;
						crc_mod_args[jp].parity_bytes = parity_bytes[n][j];
						crc_mod_args[jp].detc_cb_sizes = detc_cb_sizes;
						crc_mod_args[jp].max_rm_data_len_tx = MAX_RM_DATA_LEN_TX;

						crc_mod_args[jp].SymbolBitN = SymbolBitN[n][i];
						crc_mod_args[jp].modSymNum = cr_symnum_tx[n][j];

						crc_mod_args[jp].package = modsymb[n] + s;

						s += cr_symnum_tx[n][j];

						crc_mod_args[jp].ServiceEN = ServiceEN_tx;
						crc_mod_args[jp].ServiceEN_index = n * TASK_NUM_TX + 2;

						pool_add_task(crc_mod, (void *)&crc_mod_args[jp], 2);
					}
				}
				ServiceEN_tx[n * TASK_NUM_TX + 1] = 0; //关闭任务添加器(n,2)
			}
			//  1.3  packing
			if (ServiceEN_tx[n * TASK_NUM_TX + 2] == cbtaskNum[n] && cbtaskNum[n] != 0)
			{ //任务添加器(n,3)打开
				if (TIME_EN == 1)
				{
					TIME++;
					//if(TIME == testTime) gettimeofday(&tx_end, NULL);//______________________rx
					gettimeofday(&tx_end, NULL); //______________________rx
					runtime.tx += (tx_end.tv_sec - tx_begin.tv_sec) + (tx_end.tv_usec - tx_begin.tv_usec) / 1000000.0;
				}
				if (count_ms == 0)
				{
					SNR = SNR + STEP;
					if (SNR > SNR_MAX)
						SNR = SNR_MIN;
					//printf("\nSNR = %f\n",SNR);
					//srand(seed);
				}
				count_ms++;
				if (count_ms == testError)
					count_ms = 0;
				m = 0;
				k = 0;
				int s = 0;
				for (int ns = 0; ns < SYMBOL_NUM; ns++)
				{
					if ((ns != 3) && (ns != 10))
					{
						for (int nc = 0; nc < CARRIER_NUM; nc++)
						{
							for (int i = 0; i < LayerNum; i++)
							{
								m = ns * CARRIER_NUM * LayerNum + nc * LayerNum + i;
								if (k < MAX_SYMBOL_NUM)
								{
									x[index_tx_write][m] = modsymb[n][i * MAX_SYMBOL_NUM + k];
								}
							}
							k++;
						}
					}
					else
					{
						for (int nc = 0; nc < CARRIER_NUM; nc++)
						{
							for (int i = 0; i < LayerNum; i++)
							{
								m = ns * CARRIER_NUM * LayerNum + nc * LayerNum + i;
								x[index_tx_write][m] = PilotSymb[LayerNum - 1][s++];
							}
						}
					}
				}
				//printf("\ntx%d store modsymb %d ", n, index_tx_write);
				//for (int i = 0; i < LayerNum; i++){
				//	printf("%d ", data_len_tx[n][i]);
				//}
				//for (int i = 0; i < LayerNum; i++){
				//	printf("\ntx %d layer %d data : ", index_tx_write, i);
				//	for (int k = 0; k < 10; k++) printf("%d",data_tx[n][i][k]);
				//}

				//----------通过信道----------
				if (t == 1)
					t = 0;
				sigma = sqrt(1 / pow(10, SNR / 10));
				//printf("SNR : %f sigma2 : %f",SNR,sigma*sigma);
				//sleep(1);
				float E = 0, Ep = 0, En = 0;
				float Er[8];
				bzero(Er, sizeof(float) * 8);
				for (int ns = 0; ns < SYMBOL_NUM; ns++)
				{
					for (int nc = 0; nc < CARRIER_NUM; nc++)
					{

						for (int nl = 0; nl < LayerNum; nl++)
						{
							for (int nr = 0; nr < RX_ANT_NUM; nr++)
							{
								H_temp[nl * RX_ANT_NUM + nr] = H[t][CARRIER_NUM * RX_ANT_NUM * LayerNum * ns + RX_ANT_NUM * LayerNum * nc + nl * RX_ANT_NUM + nr];
							}
						}
						for (int i = 0; i < LayerNum; i++)
						{
							x_temp[i] = x[index_tx_write][CARRIER_NUM * LayerNum * ns + LayerNum * nc + i];
						}
						cblas_cgemm(Layout, transa, transb, _m, _n, _k, &alpha, H_temp, lda, x_temp, ldb, &beta, y_temp, ldc);
						for (int i = 0; i < RX_ANT_NUM; i++)
						{
							y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real = y_temp[i].real + sigma * gwn[CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real;
							y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag = y_temp[i].imag + sigma * gwn[CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag;
							//if(nc == 144) printf("\nRX %d symbol %d Carrier %d : y_temp = %f+%fi, y = %f+%fi, noise = %f+%fi\n",i,ns,nc, y_temp[i].real,y_temp[i].imag,y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real,y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag,y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real - y_temp[i].real,y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag - y_temp[i].imag);
							E += y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real * y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real + y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag * y[index_tx_write][CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag;
							Ep += y_temp[i].real * y_temp[i].real + y_temp[i].imag * y_temp[i].imag;
							Er[i] += y_temp[i].real * y_temp[i].real + y_temp[i].imag * y_temp[i].imag;
							En += sigma * gwn[CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real * sigma * gwn[CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].real + sigma * gwn[CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag * sigma * gwn[CARRIER_NUM * RX_ANT_NUM * ns + RX_ANT_NUM * nc + i].imag;
						}
					}
				}
				E /= SYMBOL_NUM * CARRIER_NUM;
				Ep /= SYMBOL_NUM * CARRIER_NUM;
				En /= SYMBOL_NUM * CARRIER_NUM;
				//printf("\nSNR = %f sigma = %f E = %f Ep = %f En = %f",SNR, sigma, E ,Ep ,En);
				for (int i = 0; i < RX_ANT_NUM; i++)
				{
					Er[i] /= SYMBOL_NUM * CARRIER_NUM;
					//printf("%f ",Er[i]);
				}
				//printf("\n");
				//printf("Through the channel...\n");
				package_tx[index_tx_write].y = y[index_tx_write];
				for (int i = 0; i < LayerNum; i++)
				{
					package_tx[index_tx_write].tbs[i] = data_len_tx[n][i];
					package_tx[index_tx_write].CQI_index[i] = CQI_index[n * MAX_BEAM + i];
					package_tx[index_tx_write].SNR = SNR;
					for (int j = 0; j < data_len_tx[n][i]; j++)
					{
						package_tx[index_tx_write].data[i][j] = data_tx[n][i][j];
					}
				}

				if (TIME_EN == 1)
				{ //---------------------Throughput information
					//TIME++;
					for (int i = 0; i < LayerNum; i++)
						bitsNum += data_len_tx[n][i];
					if (TIME == testTime)
					{
						//gettimeofday(&tx_end, NULL);//______________________tx
						//runtime.tx += (tx_end.tv_sec - tx_begin.tv_sec) + (tx_end.tv_usec - tx_begin.tv_usec) / 1000000.0;
						printf("\n--------------------TX : Time statistics--------------------\n");
						printf("Amount of information    : %-8.4fMbit\n", bitsNum / 1024.0 / 1024);
						printf("TX time                  : %-8.4fs\n", runtime.tx);
						printf("Throughput               : %-8.4fMbps\n", bitsNum / runtime.tx / 1024 / 1024);
						runtime.tx = 0;
						//gettimeofday(&tx_begin, NULL);//--------------------tx
						bitsNum = 0;
						TIME = 0;
					}
					gettimeofday(&tx_begin, NULL); //--------------------tx
				}

				ServiceEN_tx[n * TASK_NUM_TX + 2] = 0; //关闭任务添加器(n,3)
				cbtaskNum[n] = 0;
				//printf("\nTX task 2");
				ServiceEN_tx[n * TASK_NUM_TX]++;
				pthread_mutex_lock(&mutex_readyNum_tx);
				readyNum_tx++;
				pthread_mutex_unlock(&mutex_readyNum_tx);
				sem_post(&cache_tx);
				index_tx_write++;
				if (index_tx_write >= PACK_CACHE)
					index_tx_write = 0;
			}
		}

		//sleep(2);
	}
	sem_post(&tx_can_be_destroyed);
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
void TaskScheduler_rx(void *arg)
{

	//--------------------Initialize parameters--------------------
	//-----simulation-----
	index_rx_read = 0;
	for (int p = 0; p < PACK_CACHE; p++)
	{
		package_rx[p].tbs = (int *)malloc(sizeof(int) * MAX_BEAM);
		package_rx[p].CQI_index = (int *)malloc(sizeof(int) * MAX_BEAM);
		package_rx[p].y = (lapack_complex_float *)malloc(SIZE_Y);
		for (int j = 0; j < MAX_BEAM; j++)
			package_rx[p].data[j] =
				(uint8_t *)malloc(sizeof(uint8_t) * MAX_DATA_LEN_TX +
								  CRC_LENGTH);
	}

	struct test_rx_t *test_rx = (struct test_rx_t *)malloc(sizeof(struct test_rx_t) * PARA_NUM_RX);

	//  1.1  Unpacking
	lapack_complex_float *package[PARA_NUM_RX];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		package[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM);
	}
	srslte_cbsegm_t *cb_rx[PARA_NUM_RX][MAX_BEAM];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			cb_rx[i][j] = (srslte_cbsegm_t *)malloc(sizeof(srslte_cbsegm_t));
		}
	}
	// int index_tx_write = 1, index_rx_read = 0;
	//printf("Unpacking initialized \n");
	//  1.2  channel estimating - signal detecting
	lapack_complex_float *CFR_SB[PARA_NUM_RX][SBNum];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < SBNum; j++)
		{
			CFR_SB[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM / SBNum);
		}
	}
	int LayerNum = LAYER_NUM;
	int inter_freq = LayerNum;
	while (CARRIER_NUM / SBNum % inter_freq != 0)
		inter_freq++;
	struct chest_calsym_args_t *chest_calsym_args = (struct chest_calsym_args_t *)malloc(sizeof(struct chest_calsym_args_t) * SBNum * PARA_NUM_RX);
	lapack_complex_float *y[PARA_NUM_RX][SBNum];
	float *R_DCT[PARA_NUM_RX][SBNum];
	lapack_complex_float *H_est[PARA_NUM_RX][SBNum];
	lapack_complex_float *x_est[PARA_NUM_RX][SBNum];
	lapack_complex_float *PilotSymb[MAX_BEAM];
	for (int i = 0; i < MAX_BEAM; i++)
	{
		PilotSymb[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * (i + 1) * CARRIER_NUM * PILOT_SYM_NUM);
		CalPilotSymb(i + 1, CARRIER_NUM, 2, 1, inter_freq, PilotSymb[i]);
	}
	lapack_complex_float *PilotSymb_SB[PARA_NUM_RX][SBNum];
	float *SymbVar[PARA_NUM_RX][SBNum];
	float *SINR_est[PARA_NUM_RX][SBNum];
	int16_t *LLRD_Package[PARA_NUM_RX];
	int *SymbolBitN_L[PARA_NUM_RX];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		LLRD_Package[i] = (int16_t *)malloc(sizeof(int16_t) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM * 6);
		SymbolBitN_L[i] = (int *)malloc(sizeof(int) * MAX_BEAM);
	}

	struct chestLS_t *chestLS_p[PARA_NUM_RX][SBNum];
	struct chestDCT_t *chestDCT_p[PARA_NUM_RX][SBNum];
	struct CalSymb_t *CalSymb_p[PARA_NUM_RX][SBNum];
	struct Rest_t *Rest_p[PARA_NUM_RX][SBNum];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < SBNum; j++)
		{
			y[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * CARRIER_NUM * SYMBOL_NUM / SBNum);
			R_DCT[i][j] = (float *)malloc(sizeof(float) * RX_ANT_NUM * LayerNum * CARRIER_NUM * SYMBOL_NUM / SBNum);
			H_est[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RX_ANT_NUM * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM / SBNum);
			x_est[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM / SBNum);
			PilotSymb_SB[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MAX_BEAM * CARRIER_NUM * PILOT_SYM_NUM / SBNum);
			SymbVar[i][j] = (float *)malloc(sizeof(float) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM / SBNum);
			SINR_est[i][j] = (float *)malloc(sizeof(float) * MAX_BEAM * CARRIER_NUM * SYMBOL_NUM / SBNum);

			chestLS_p[i][j] = (struct chestLS_t *)malloc(sizeof(struct chestLS_t));
			chestLS_init(chestLS_p[i][j], CARRIER_NUM / SBNum, inter_freq, SYMBOL_NUM);
			chestDCT_p[i][j] = (struct chestDCT_t *)malloc(sizeof(struct chestDCT_t));
			chestDCT_init(chestDCT_p[i][j], CARRIER_NUM / SBNum, inter_freq, SYMBOL_NUM);
			CalSymb_p[i][j] = (struct CalSymb_t *)malloc(sizeof(struct CalSymb_t));
			CalSymb_init(CalSymb_p[i][j], LayerNum, RX_ANT_NUM);
			Rest_p[i][j] = (struct Rest_t *)malloc(sizeof(struct Rest_t));
			REstimator_init(Rest_p[i][j], CARRIER_NUM / SBNum, inter_freq, SYMBOL_NUM);
		}
	}
	int PilotSymbIndex[2];
	PilotSymbIndex[0] = 3;
	PilotSymbIndex[1] = 10;

	printf("RX task1 initialized \n");
	//  1.3  descrambling - demodulation - rate matching - turbo decoding - crc check
	const int cbNum = (MAX_DATA_LEN_RX + CRC_LENGTH) / 6144 + 1;
	uint32_t max_rm_data_len_rx = MAX_SYMBOL_NUM * CQI_mod[15];
	int max_modsymNum = MAX_SYMBOL_NUM;
	int cbtaskNum[PARA_NUM_RX];
	int *CQI_index = (int *)malloc(sizeof(int) * MAX_BEAM * PACK_CACHE);

	struct derm_crc_args_t *derm_crc_args = (struct derm_crc_args_t *)malloc(sizeof(struct derm_crc_args_t) * MAX_BEAM * cbNum * PARA_NUM_RX);
	int modsymNum[PARA_NUM_RX][MAX_BEAM * cbNum];
	int cr_symnum_rx[PARA_NUM_RX][MAX_BEAM * cbNum];
	int SymbolBitN[PARA_NUM_RX][MAX_BEAM];
	uint32_t rm_outlen_rx[PARA_NUM_RX][MAX_BEAM];
	uint32_t rm_data_len_rx[PARA_NUM_RX][MAX_BEAM];
	uint32_t Lamda;

	genQAMtable();

	int Kr = 0;
	int16_t *cr_tdec_data_i_rx[PARA_NUM_RX][MAX_BEAM * cbNum];
	srslte_tdec_t *tdec[PARA_NUM_RX][MAX_BEAM * cbNum];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < MAX_BEAM * cbNum; j++)
		{
			cr_tdec_data_i_rx[i][j] = (int16_t *)malloc(sizeof(int16_t) * 6176 * 3);
			tdec[i][j] = (srslte_tdec_t *)malloc(sizeof(srslte_tdec_t));
			srslte_tdec_init(tdec[i][j], SRSLTE_TCOD_MAX_LEN_CB);
		}
	}
	int *detc_cb_sizes = (int *)malloc(sizeof(int) * 6145);
	for (int i = 1; i <= 6144; i++)
	{
		detc_cb_sizes[i] = 0;
	}
	for (int i = 0; i < 188; i++)
	{
		detc_cb_sizes[srslte_cbsegm_cbsize(i)] = i;
	}

	uint8_t *cr_data_bytes_rx[PARA_NUM_RX][MAX_BEAM * cbNum];
	srslte_crc_t *crc_cb[PARA_NUM_RX][MAX_BEAM * cbNum];
	uint32_t *checksum_cb[PARA_NUM_RX][MAX_BEAM];
	uint8_t *data_rx[PARA_NUM_RX][MAX_BEAM];
	uint8_t *data_bytes_rx[PARA_NUM_RX][MAX_BEAM];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < MAX_BEAM * cbNum; j++)
		{
			crc_cb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_cb[i][j], CRC_24B, CRC_LENGTH);
			cr_data_bytes_rx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB / 8);
		}
	}
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			data_rx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (MAX_DATA_LEN_RX + CRC_LENGTH));
			data_bytes_rx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (MAX_DATA_LEN_RX + CRC_LENGTH) / 8);
			checksum_cb[i][j] = (uint32_t *)malloc(sizeof(uint32_t) * cbNum);
			for (int k = 0; k < cbNum; k++)
			{
				checksum_cb[i][j][k] = 0;
			}
		}
	}

	//cbcon
	int data_len_rx[PARA_NUM_RX][MAX_BEAM];
	printf("RX task2 initialized \n");
	//  1.4  code block desegmentation - crc check
	struct crc_check_args_t *crc_check_args = (struct crc_check_args_t *)malloc(sizeof(struct crc_check_args_t) * MAX_BEAM * PARA_NUM_RX);
	srslte_crc_t *crc_tb[PARA_NUM_RX][MAX_BEAM];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < MAX_BEAM; j++)
		{
			crc_tb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_tb[i][j], CRC_24A, CRC_LENGTH);
		}
	}
	uint32_t *checksum_tb[PARA_NUM_RX];
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		checksum_tb[i] = (uint32_t *)malloc(sizeof(uint32_t) * MAX_BEAM);
	}
	printf("RX task3 initialized \n");
	//-----Blocks error statistics-----
	struct BER_t *BER = (struct BER_t *)malloc(sizeof(struct BER_t));
	BER_init(BER, MAX_BEAM);
	int *BERsignal = (int *)malloc(sizeof(int) * PARA_NUM_RX);
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		BERsignal[i] = 0;
	}
	int statistics = 1;
	int BLER_EN = 1, BER_EN = 1;
	int TIME_EN = 1;

	printf("TaskScheduler rx prepared...\n");
	int count_ms = 0, count_s = 0, TIME = 0;
	//-----time test-----
	struct timeval rx_begin, rx_end;
	runtime.rx = 0;
	int bitsNum = 0;
	//--------------------Data processing--------------------
	ServiceEN_rx = (int *)malloc(sizeof(int) * PARA_NUM_RX * TASK_NUM_RX);
	for (int i = 0; i < PARA_NUM_RX; i++)
	{
		for (int j = 0; j < TASK_NUM_RX; j++)
		{
			ServiceEN_rx[i * TASK_NUM_RX + j] = 0;
		}
	}
	for (int i = 0; i < PARA_NUM_RX; i++)
		ServiceEN_rx[TASK_NUM_RX * i] = LayerNum;

	sem_post(&rx_prepared);
	sem_post(&rx_prepared);
	sem_wait(&tx_prepared);
	sem_wait(&rx_buff_prepared);

	// omp_set_num_threads(1);
	if (TIME_EN == 1)
		gettimeofday(&rx_begin, NULL); //--------------------rx
	sem_wait(&cache_rx);
	static int cnt = 0;
	while (1)
	{

		//sleep(1);
		//if(runIndex == 3000) break;
		for (int n = 0; n < PARA_NUM_RX; n++)
		{
			// printf("n:%d\n",n);

			//  1.2  channel estimating - signal detecting
			if (ServiceEN_rx[n * TASK_NUM_RX] == LayerNum && readyNum_rx > 0)
			{ //有数据需要处理，且任务添加器(n,1)打开
				// if (readyNum_tx > 1)
				// 	index_rx_read = index_tx_write;
				// else
				// 	index_rx_read = 0;
				// printf("CNT:%d,tx_write:%d,tx_read:%d,rx_write:%d,rx_read:%d\n",
				// 	   ++cnt, index_tx_write, index_tx_read, index_rx_write, index_rx_read);
				// printf("\tstart_tx:%d,ready_tx:%d,start_rx:%d,ready_rx:%d,empty:%d\n",
				// 	   startNum_tx, readyNum_tx, startNum_rx, readyNum_rx, buffisEmpty);
				for (int i = 0; i < LayerNum; ++i)
				{
					srslte_cbsegm(cb_rx[n][i], package_rx[index_rx_read].tbs[i]);
					CQI_index[n * MAX_BEAM + i] = package_rx[index_rx_read].CQI_index[i];
					test_rx[n].data[i] = package_rx[index_rx_read].data[i];
					SymbolBitN_L[n][i] = CQI_mod[CQI_index[n * MAX_BEAM + i]];
					// test_rx[n].packIdx = index_rx_read;
				}
				test_rx[n].SNR = package_rx[index_rx_read].SNR;
				for (int i = 0; i < SBNum; ++i)
				{
					int j = n * SBNum + i;
					chest_calsym_args[j].package = package_rx[index_rx_read].y;
					chest_calsym_args[j].SBindex = i;
					chest_calsym_args[j].SBNum = SBNum;

					chest_calsym_args[j].SignalRec = y[n][i];
					chest_calsym_args[j].PilotSymb_SB = PilotSymb_SB[n][i];
					chest_calsym_args[j].PilotSymb = PilotSymb[LayerNum - 1];

					chest_calsym_args[j].R_DCT = R_DCT[n][j];
					chest_calsym_args[j].PilotSymbIndex = PilotSymbIndex;
					chest_calsym_args[j].TxAntNum = TX_ANT_NUM;
					chest_calsym_args[j].RxAntNum = RX_ANT_NUM;
					chest_calsym_args[j].CarrierNum_SB = CARRIER_NUM / SBNum;
					chest_calsym_args[j].CarrierNum = CARRIER_NUM;
					chest_calsym_args[j].UserNum = USER_NUM;
					chest_calsym_args[j].MaxBeam = MAX_BEAM;
					chest_calsym_args[j].PilotSymbNum = PILOT_SYM_NUM;
					chest_calsym_args[j].SymbNum = SYMBOL_NUM;
					chest_calsym_args[j].inter_freq = inter_freq;
					chest_calsym_args[j].LayerNum = LayerNum;
					chest_calsym_args[j].Step = 1;
					chest_calsym_args[j].flg_ave = 1;
					chest_calsym_args[j].ChEstType = 0;
					chest_calsym_args[j].SINR_est = SINR_est[n][i];
					chest_calsym_args[j].chestLS_p = chestLS_p[n][i];
					chest_calsym_args[j].chestDCT_p = chestDCT_p[n][i];
					chest_calsym_args[j].CalSymb_p = CalSymb_p[n][i];
					chest_calsym_args[j].Rest_p = Rest_p[n][i];

					chest_calsym_args[j].CFR_est = H_est[n][i];
					chest_calsym_args[j].CFR = H[0];
					chest_calsym_args[j].CFR_SB = CFR_SB[n][i];

					chest_calsym_args[j].SymbEst = x_est[n][i];
					chest_calsym_args[j].SymbVar = SymbVar[n][i];

					chest_calsym_args[j].LLRD_Package = LLRD_Package[n];
					chest_calsym_args[j].SymbolBitN_L = SymbolBitN_L[n];

					chest_calsym_args[j].ServiceEN_index = n * TASK_NUM_RX + 1;
					chest_calsym_args[j].ServiceEN_rx = ServiceEN_rx;

					pool_add_task(chest_calsym, (void *)&chest_calsym_args[j], 3);
				}
				ServiceEN_rx[n * TASK_NUM_RX] = 0;
				//printf("\nrx%d get package %d", n, index_tx_write);
				//if (index_rx_read != 0)
				//{
				pthread_mutex_lock(&mutex_readyNum_rx);
				readyNum_rx--;
				pthread_mutex_unlock(&mutex_readyNum_rx);
				index_rx_read++;
				//}
				if (index_rx_read >= PACK_CACHE)
					index_rx_read = 0;
			}

			//  1.3  descrambling - demodulation - rate matching - turbo decoding - crc check
			if (ServiceEN_rx[n * TASK_NUM_RX + 1] == SBNum)
			{

				//printf("\nrx%d x_est : ",n);
				//for(int i = 0; i < 8; i++) printf("%f+%fi ",x_est[n][0][i]);

				int s = 0;
				cbtaskNum[n] = 0;
				for (int i = 0; i < LayerNum; i++)
				{
					int c = 0;
					// printf("1i = %d\n", i);
					rm_data_len_rx[n][i] = MAX_SYMBOL_NUM * CQI_mod[CQI_index[n * MAX_BEAM + i]];
					// printf("2i = %d\n", i);
					Lamda = MAX_SYMBOL_NUM % cb_rx[n][i]->C;
					// printf("3i = %d\n", i);
					cbtaskNum[n] += cb_rx[n][i]->C;
					// printf("4i = %d\n", i);
					for (int r = 0; r < (cb_rx[n][i]->C); ++r)
					{
						int j = i * cbNum + r;
						int jp = n * LayerNum * cbNum + i * cbNum + r;
						if (r < cb_rx[n][i]->C - Lamda)
							rm_outlen_rx[n][i] = CQI_mod[CQI_index[n * MAX_BEAM + i]] * (MAX_SYMBOL_NUM / cb_rx[n][i]->C);
						else
							rm_outlen_rx[n][i] = CQI_mod[CQI_index[n * MAX_BEAM + i]] * ((MAX_SYMBOL_NUM - 1) / cb_rx[n][i]->C + 1);
						Kr = (r < cb_rx[n][i]->C2) ? cb_rx[n][i]->K2 : cb_rx[n][i]->K1;
						SymbolBitN[n][i] = CQI_mod[CQI_index[n * MAX_BEAM + i]];
						cr_symnum_rx[n][j] = rm_outlen_rx[n][i] / SymbolBitN[n][i];

						derm_crc_args[jp].LLRD_Package = LLRD_Package[n];

						derm_crc_args[jp].modSymNum = cr_symnum_rx[n][j];
						derm_crc_args[jp].symindex = s;
						s += cr_symnum_rx[n][j];
						derm_crc_args[jp].SymbolBitN = SymbolBitN[n][i];
						derm_crc_args[jp].LayerNum = 1;

						derm_crc_args[jp].cbs_rm = rm_outlen_rx[n][i];
						derm_crc_args[jp].Kr = Kr;
						derm_crc_args[jp].rv_idx = 0;
						derm_crc_args[jp].detc_cb_sizes = detc_cb_sizes;

						derm_crc_args[jp].cb_tdec = cr_tdec_data_i_rx[n][j];
						derm_crc_args[jp].tdec = tdec[n][j];
						derm_crc_args[jp].nof_iterations = 4;
						derm_crc_args[jp].cb_crc = cr_data_bytes_rx[n][j];

						derm_crc_args[jp].crc_p = crc_cb[n][j];
						derm_crc_args[jp].crc_poly = CRC_24B;
						derm_crc_args[jp].crc_length = CRC_LENGTH;
						derm_crc_args[jp].checksum = checksum_cb[n][i];
						derm_crc_args[jp].r = r;
						derm_crc_args[jp].cbnum = cb_rx[n][i]->C;

						derm_crc_args[jp].tb = data_bytes_rx[n][i];
						derm_crc_args[jp].conindex = c;
						derm_crc_args[jp].F = cb_rx[n][i]->F;
						if ((cb_rx[n][i]->C) > 1)
						{
							if (r == 0)
								c += Kr - cb_rx[n][i]->F - CRC_LENGTH;
							else
								c += Kr - CRC_LENGTH;
						}

						derm_crc_args[jp].ServiceEN_index = n * TASK_NUM_RX + 2;
						derm_crc_args[jp].ServiceEN_rx = ServiceEN_rx;

						FILE *fpl = fopen("llrd.txt", "a");
						fprintf(fpl, "llrd:\n");
						for (int k = 0; k < CARRIER_NUM * SYMBOL_NUM * 6; k++)
						{
							for (int l = 0; l < MAX_BEAM; l++)
								fprintf(fpl, "%d, ", LLRD_Package[n][k * 8 + l]);
							fprintf(fpl, "\n");
						}
						fprintf(fpl, "\n");
						fclose(fpl);

						pool_add_task(derm_crc, (void *)&derm_crc_args[jp], 3);
						// printf("r = %d\n", r);
					}
					// printf("i = %d\n\n", i);
				}
				ServiceEN_rx[n * TASK_NUM_RX + 1] = 0;
			}
			// printf("\n%d = %d?",ServiceEN_rx[n * TASK_NUM_RX + 2],cbtaskNum[n]);

			//  1.4  code block desegmentation - crc check
			if (ServiceEN_rx[n * TASK_NUM_RX + 2] == cbtaskNum[n] && cbtaskNum[n] != 0)
			{
				for (int i = 0; i < LayerNum; i++)
				{
					int j = n * LayerNum + i;
					data_len_rx[n][i] = cb_rx[n][i]->tbs;
					//printf("\nLayer %d data length : %d", i, data_len_rx[n][i]);
					//printf("\nrx %d layer %d data : ", n, i);
					//for(int k = 0; k < 10; k++) printf("%d", data_rx[n][i][k]);
					crc_check_args[j].crc_p = crc_tb[n][i];
					crc_check_args[j].crc_poly = CRC_24A;
					crc_check_args[j].crc_length = CRC_LENGTH;
					crc_check_args[j].tb = data_bytes_rx[n][i];
					crc_check_args[j].checksum = checksum_tb[n];
					crc_check_args[j].layer = i;
					crc_check_args[j].tbs = data_len_rx[n][i];

					crc_check_args[j].subframeIndex = n;
					crc_check_args[j].BERsignal = BERsignal;

					FILE *fpdb = fopen("datab.txt", "a");
					fprintf(fpdb, "datab:\n");
					for (int k = 0; k < (MAX_DATA_LEN_RX + CRC_LENGTH) / 8; k++)
					{
						for (int l = 0; l < MAX_BEAM; l++)
							fprintf(fpdb, "%d, ", data_bytes_rx[n][i][k]);
						fprintf(fpdb, "\n");
					}
					fprintf(fpdb, "\n");
					fclose(fpdb);

					pool_add_task(crc_check, (void *)&crc_check_args[j], 3);
				}
				ServiceEN_rx[n * TASK_NUM_RX + 2] = 0;
				cbtaskNum[n] = 0;
				//printf("\nrx%d finish a package",n);
			}

			//  1.5  BER statistics
			if (BERsignal[n] == LayerNum)
			{
				if (statistics == 1)
				{ //----------------------Statistics information
					if (TIME_EN == 1)
					{
						TIME++;
						//if(TIME == testTime) gettimeofday(&rx_end, NULL);//______________________rx
						gettimeofday(&rx_end, NULL); //______________________rx
						runtime.rx += (rx_end.tv_sec - rx_begin.tv_sec) + (rx_end.tv_usec - rx_begin.tv_usec) / 1000000.0;
					}
					if (BLER_EN == 1)
					{
						for (int i = 0; i < LayerNum; i++)
						{
							BER->BlocksNum++;
							BER->BlocksNum_L[i]++;
							if (checksum_tb[n][i] != 0)
							{
								BER->BLERNum++;
								BER->BLERNum_L[i]++;
							}
						}
					}
					if (BER_EN == 1)
					{
						for (int i = 0; i < LayerNum; i++)
						{
							srslte_bit_unpack_vector(data_bytes_rx[n][i], data_rx[n][i], data_len_rx[n][i]);
							for (int j = 0; j < cb_rx[n][i]->tbs; j++)
							{
								BER->BitsNum++;
								BER->BitsNum_L[i]++;
								if (test_rx[n].data[i][j] != data_rx[n][i][j])
								{
									BER->BERNum++;
									BER->BERNum_L[i]++;
								}
							}
						}
					}
					if (BLER_EN == 1 || BER_EN == 1)
					{ //------Error information
						count_ms++;

						if (count_ms == testError)
						{
							printf("\n--------------------No.%d~%d--------------------", runIndex + 2 - testError, runIndex + 1);
							count_ms = 0;
							if (BLER_EN == 1)
							{ //-------------Block error information
								printf("\n--------------------Block Error statistics(SNR %5.2f)--------------------", test_rx[n].SNR);
								printf("\nBlock error : %f(%d/%d)", BER->BLERNum * 1.0 / BER->BlocksNum, BER->BLERNum, BER->BlocksNum);
								for (int i = 0; i < LayerNum; i++)
								{
									printf("\nLayer %d Block error : %f(%d/%d)", i, BER->BLERNum_L[i] * 1.0 / BER->BlocksNum_L[i], BER->BLERNum_L[i], BER->BlocksNum_L[i]);
								}

								BER->BlocksNum = 0;
								BER->BLERNum = 0;
								for (int i = 0; i < LayerNum; i++)
								{
									BER->BlocksNum_L[i] = 0;
									BER->BLERNum_L[i] = 0;
								}
							}
							if (BER_EN == 1)
							{ //--------------Bits error information
								printf("\n--------------------Bits Error statistics(SNR %5.2f)--------------------", test_rx[n].SNR);
								printf("\nBits error : %f(%d/%d)", BER->BERNum * 1.0 / BER->BitsNum, BER->BERNum, BER->BitsNum);
								for (int i = 0; i < LayerNum; i++)
								{
									printf("\nLayer %d Bits error : %f(%d/%d)", i, BER->BERNum_L[i] * 1.0 / BER->BitsNum_L[i], BER->BERNum_L[i], BER->BitsNum_L[i]);
								}

								BER->BitsNum = 0;
								BER->BERNum = 0;
								for (int i = 0; i < LayerNum; i++)
								{
									BER->BitsNum_L[i] = 0;
									BER->BERNum_L[i] = 0;
								}
							}
						}
					}
					if (TIME_EN == 1)
					{ //---------------------Throughput information
						//TIME++;
						for (int i = 0; i < LayerNum; i++)
							bitsNum += data_len_rx[n][i];
						if (TIME == testTime)
						{
							//gettimeofday(&rx_end, NULL);//______________________rx
							//runtime.rx += (rx_end.tv_sec - rx_begin.tv_sec) + (rx_end.tv_usec - rx_begin.tv_usec) / 1000000.0;
							printf("\n--------------------RX : Time statistics--------------------\n");
							printf("Amount of information : %-8.4fMbit\n", bitsNum / 1024.0 / 1024);
							printf("RX time               : %-8.4fs\n", runtime.rx);
							printf("Throughput            : %-8.4fMbps\n", bitsNum / runtime.rx / 1024 / 1024);
							runtime.rx = 0;
							//gettimeofday(&rx_begin, NULL);//--------------------rx
							bitsNum = 0;
							TIME = 0;
							//printf("startNum_tx %d\n",startNum_tx);
							//printf("readyNum_tx %d\n",readyNum_tx);
						}
						gettimeofday(&rx_begin, NULL); //--------------------rx
					}
					runIndex++;
				}
				BERsignal[n] = 0;
				ServiceEN_rx[n * TASK_NUM_RX] = LayerNum;
				pthread_mutex_lock(&mutex_startNum_rx);
				startNum_rx--;
				pthread_mutex_unlock(&mutex_startNum_rx);
			}
		}
	}
	sem_post(&rx_can_be_destroyed);
	freeQAMtable();
}

/*******************************************************************************************************/
/************************************************Tx_buff************************************************/
/*******************************************************************************************************/

uint8_t *mbuf;
// extern uint8_t *mbuf;

// int index_tx_read;
// extern int readyNum_tx;
// extern int startNum_tx;
// extern struct package_t *package_tx;
// extern pthread_mutex_t mutex_readyNum_tx;
// extern pthread_mutex_t mutex_startNum_tx;
extern sem_t tx_buff_can_be_destroyed;
// extern sem_t tx_prepared;
// extern sem_t tx_buff_prepared;
extern sem_t buffisnotEmpty;

// int buffisEmpty;
pthread_mutex_t mutex_buffisEmpty;

int package_to_buff(struct package_t *package, uint8_t *buff_p);

void Tx_buff(void *arg)
{
	// printf("tx buff start\n");
	index_tx_read = 0;
	buffisEmpty = 1;
	mbuf = (unsigned char *)malloc(MAX_MBUFF);
	pthread_mutex_init(&mutex_buffisEmpty, NULL);
	printf("Tx Buff prepared...\n");
	sem_post(&tx_buff_prepared);
	sem_wait(&tx_prepared);
	// printf("tx buff start\n");
	while (1)
	{
		// printf("aaaaaa……\n");
		if (readyNum_tx == 0)
			sem_wait(&cache_tx);
		if (readyNum_tx > 0 && buffisEmpty)
		{
			// printf("\ntx buff circle start:%d\n", index_tx_read);
			package_to_buff(&package_tx[index_tx_read], mbuf);
			// printf("\ntx buff down:%d\n", index_tx_read);
			index_tx_read++;
			if (index_tx_read >= PACK_CACHE)
				index_tx_read = 0;
			pthread_mutex_lock(&mutex_readyNum_tx);
			readyNum_tx--;
			pthread_mutex_unlock(&mutex_readyNum_tx);
			pthread_mutex_lock(&mutex_startNum_tx);
			startNum_tx--;
			pthread_mutex_unlock(&mutex_startNum_tx);
			pthread_mutex_lock(&mutex_buffisEmpty);
			buffisEmpty = 0;
			pthread_mutex_unlock(&mutex_buffisEmpty);
			sem_post(&buffisnotEmpty);
			// printf("buffisEmpty:%d\n", buffisEmpty);
			// printf("tx buff circle end\n");
		}
	}
	pthread_mutex_destroy(&mutex_buffisEmpty);
	sem_post(&tx_buff_can_be_destroyed);
}

int package_to_buff(struct package_t *package, uint8_t *buff)
{
	// printf("package to buff start\n");
	uint8_t *buff_p = buff;
	int buff_length = 0;
	// printf("size of tbs:%ld\n", sizeof(package->tbs));

	uint8_t *tbs_p = (uint8_t *)(package->tbs);
	for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
	{
		*buff_p = *tbs_p;
		buff_p++;
		tbs_p++;
		buff_length++;
		// for (int j = sizeof(int) - 1; j >= 0; j--)
		// {
		//     printf("%d\n", j);
		//     *buff_p = package->tbs[i];
		// }
	}
	// printf("tbs done\n");

	uint8_t *cqi_p = (uint8_t *)(package->CQI_index);
	for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
	{
		*buff_p = *cqi_p;
		buff_p++;
		cqi_p++;
		buff_length++;
	}
	// printf("cqi done\n");

	uint8_t *snr_p = (uint8_t *)(&(package->SNR));
	for (int i = 0; i < sizeof(float); i++)
	{
		*buff_p = *snr_p;
		buff_p++;
		snr_p++;
		buff_length++;
	}
	// printf("snr done\n");

	uint8_t *y_p = (uint8_t *)(package->y);
	for (int i = 0; i < SIZE_Y; i++)
	{
		*buff_p = *y_p;
		buff_p++;
		y_p++;
		buff_length++;
	}
	// printf("y done\n");
	uint8_t *data_p;
	// printf("buff_length:%d\n",buff_length);
	unsigned char tem;
	for (int j = 0; j < MAX_BEAM; j++)
	{
		data_p = (uint8_t *)(package->data[j]);
		for (int i = 0; i < MAX_DATA_LEN_TX + CRC_LENGTH; i++)
		{
			// tem = 0xFF;
			// *buff_p = tem;
			*buff_p = package->data[j][i];
			buff_p++;
			data_p++;
			// printf("buff_length:%d\n", buff_length);
			buff_length++;
		}
		// printf("data%d done\n", j);

	} // printf("data done\n");
	return buff_length;
}

/**************************************************************************/
/*****************************Rx_buff**************************************/
/**************************************************************************/
extern sem_t rx_can_be_destroyed;
// extern sem_t rx_buff_prepared;
// pthread_mutex_t mutex_readyNum_rx;
// int index_rx_write;
// int readyNum_rx;
// struct package_t package_rx[PACK_CACHE];
int buff_to_package(struct package_t *package, unsigned char *buff_p);
// FILE *rx_buff_file;

void Rx_buff(void *arg)
{
	// rx_buff_file = fopen("Rx_buff_log.txt", "w");
	readyNum_rx = 0, startNum_rx = 0;
	pthread_mutex_init(&mutex_readyNum_rx, NULL);
	pthread_mutex_init(&mutex_startNum_rx, NULL);
	index_rx_write = 0;
	// printf("rx buff start\n");
	printf("Rx Buff prepared...\n");
	sem_post(&rx_buff_prepared);
	sem_wait(&rx_prepared);
	// printf("rx buff start\n");
	while (1)
	{
		if (buffisEmpty)
			sem_wait(&buffisnotEmpty);
		if (startNum_rx < PACK_CACHE && (!buffisEmpty))
		{
			pthread_mutex_lock(&mutex_startNum_rx);
			startNum_rx++;
			pthread_mutex_unlock(&mutex_startNum_rx);
			// printf("\nrx buff circle start:%d\n", index_rx_write);
			// printf("startNum_rx = %d\n", startNum_rx);
			// printf("readyNum_rx = %d\n", readyNum_rx);
			buff_to_package(&package_rx[index_rx_write], mbuf);
			// printf("\nrx buff down:%d\n", index_rx_write);
			index_rx_write++;
			if (index_rx_write >= PACK_CACHE)
				index_rx_write = 0;
			pthread_mutex_lock(&mutex_readyNum_rx);
			readyNum_rx++;
			pthread_mutex_unlock(&mutex_readyNum_rx);
			sem_post(&cache_rx);
			pthread_mutex_lock(&mutex_buffisEmpty);
			buffisEmpty = 1;
			pthread_mutex_unlock(&mutex_buffisEmpty);
		}
	}
	sem_post(&rx_can_be_destroyed);
}

int buff_to_package(struct package_t *package, unsigned char *buff)
{
	unsigned char *buff_p = buff;
	int buff_length = 0;

	uint8_t *tbs_p = (uint8_t *)(package->tbs);
	for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
	{
		*tbs_p = *buff_p;
		buff_p++;
		tbs_p++;
		buff_length++;
	}
	// printf("tbs done\n");

	uint8_t *cqi_p = (uint8_t *)(package->CQI_index);
	for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
	{
		*cqi_p = *buff_p;
		buff_p++;
		cqi_p++;
		buff_length++;
	}
	// printf("cqi done\n");

	uint8_t *snr_p = (uint8_t *)(&(package->SNR));
	for (int i = 0; i < sizeof(float); i++)
	{
		*snr_p = *buff_p;
		buff_p++;
		snr_p++;
		buff_length++;
	}
	// printf("snr done\n");

	uint8_t *y_p = (uint8_t *)(package->y);
	for (int i = 0; i < SIZE_Y; i++)
	{
		*y_p = *buff_p;
		buff_p++;
		y_p++;
		buff_length++;
	}
	// printf("y done\n");
	uint8_t *data_p;
	// printf("buff_length:%d\n",buff_length);
	unsigned char tem;
	for (int j = 0; j < MAX_BEAM; j++)
	{
		// data_p = (uint8_t *)(package->data[j]);
		for (int i = 0; i < MAX_DATA_LEN_TX + CRC_LENGTH; i++)
		{
			// tem = 0xFF;
			// *buff_p = tem;
			package->data[j][i] = *buff_p;
			buff_p++;
			data_p++;
			// fprintf(rx_buff_file,"buff_length:%d\n",buff_length);
			// printf("buff_length:%d\n", buff_length);
			buff_length++;
		}
		// printf("data%d done\n", j);

	} // printf("data done\n");

	return buff_length;
}