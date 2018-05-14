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
extern sem_t tx_can_be_destroyed;
extern sem_t rx_can_be_destroyed;

const int paraNum_tx = 2;   // 发送端同时处理子帧上限
const int paraNum_rx = 5;   // 接收端同时处理子帧上限
const int CacheNum_tx = 20; // 缓存
const int CacheNum_rx = 20;
int index_write_tx;
int index_read_rx;
int readyNum_tx;
int readyNum_rx;
int startNum_tx;
pthread_mutex_t mutex_startNum_tx;
pthread_mutex_t mutex_readyNum_tx;
pthread_mutex_t mutex_readyNum_rx;
struct package_t package_tx[20];
struct package_t package_rx[20];

const int SBNum = 50; // 信道估计相关
//test
lapack_complex_float *H[1];
const int testError = 100;
const int testTime = 100;
struct RunTime runtime;

int runIndex = 0;

void TaskScheduler_tx(void *arg)
{

	//--------------------Set the initial parameters--------------------
	readyNum_tx = 0, startNum_tx = 0;
	index_write_tx = 0, index_read_rx = 0;
	pthread_mutex_init(&mutex_startNum_tx, NULL);
	pthread_mutex_init(&mutex_readyNum_tx, NULL);
	for (int p = 0; p < CacheNum_tx; p++)
	{
		package_tx[p].tbs = (int *)malloc(sizeof(int) * MaxBeam);
		package_tx[p].CQI_index = (int *)malloc(sizeof(int) * MaxBeam);
		package_tx[p].y = (lapack_complex_float *)malloc(SIZE_Y);
		for (int j = 0; j < MaxBeam; j++)
			package_tx[p].data[j] =
				(uint8_t *)malloc(sizeof(uint8_t) * MAX_DATA_SIZE_TX +
								  crc_length);
	}
	uint32_t seed = 1;
	int k = 0, t = 0;
	//seed = time(NULL);
	srand(seed);
	int LayerNum = LAYER_NUM;
	int *CQI_index = (int *)malloc(sizeof(int) * MaxBeam * CacheNum_tx);
	for (int i = 0; i < CacheNum_tx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			CQI_index[i * MaxBeam + j] = CQI_INDEX;
		}
	}
	int inter_freq = LayerNum;
	while (CarrierNum / SBNum % inter_freq != 0)
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
	int data_len_tx[paraNum_tx][MaxBeam];
	uint8_t *data_tx[paraNum_tx][MaxBeam];
	uint8_t *data_bytes_tx[paraNum_tx][MaxBeam];
	for (int i = 0; i < paraNum_tx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (MAX_DATA_SIZE_TX + crc_length));
			data_bytes_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (MAX_DATA_SIZE_TX + crc_length) / 8);
		}
	}

	//--------------------The preparation of the TX--------------------

	//  1.1  crc attach - code block segmentation
	struct crc_cbsegm_args_t *crc_cbsegm_args = (struct crc_cbsegm_args_t *)malloc(sizeof(struct crc_cbsegm_args_t) * paraNum_tx * MaxBeam);
	srslte_crc_t *crc_tb[paraNum_tx][MaxBeam];
	srslte_cbsegm_t *cb_tx[paraNum_tx][MaxBeam];
	for (int i = 0; i < paraNum_tx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			crc_tb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_tb[i][j], crc_24A, crc_length);
			cb_tx[i][j] = (srslte_cbsegm_t *)malloc(sizeof(srslte_cbsegm_t));
		}
	}
	printf("TX task1 initialized \n");
	//  1.2  crc attach - turbo coding - rate matching - code block concatenation
	const int cbNum = (MAX_DATA_SIZE_TX + crc_length) / 6144 + 1;
	srslte_crc_t *crc_cb[paraNum_tx][MaxBeam * cbNum];
	for (int i = 0; i < paraNum_tx; i++)
	{
		for (int j = 0; j < MaxBeam * cbNum; j++)
		{
			crc_cb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_cb[i][j], crc_24B, crc_length);
		}
	}
	struct crc_mod_args_t *crc_mod_args = (struct crc_mod_args_t *)malloc(sizeof(struct crc_mod_args_t) * paraNum_tx * MaxBeam * cbNum);
	uint8_t *cr_data_bytes_tx[paraNum_tx][MaxBeam * cbNum];
	srslte_tcod_t *tcod[paraNum_tx][MaxBeam * cbNum];
	uint8_t *cr_tcod_data_tx[paraNum_tx][MaxBeam * cbNum];
	uint8_t *cr_rm_data_tx[paraNum_tx][MaxBeam * cbNum];
	uint8_t *cr_rm_data_bytes_tx[paraNum_tx][MaxBeam * cbNum];
	uint8_t *w_buff_tx[paraNum_tx][MaxBeam * cbNum];
	uint8_t *parity_bytes[paraNum_tx][MaxBeam * cbNum];
	uint8_t *rm_data_tx[paraNum_tx][MaxBeam];
	uint32_t rm_data_len_tx[paraNum_tx][MaxBeam];
	uint32_t rm_outlen_tx[paraNum_tx][MaxBeam * cbNum];
	uint32_t max_rm_data_len_tx = MAX_SYMBOL_NUM * CQI_mod[15];
	uint32_t Lamda;
	int max_mod_data_len_tx = max_rm_data_len_tx / CQI_mod[1];
	int cr_symnum_tx[paraNum_tx][MaxBeam * cbNum];
	int SymbolBitN[paraNum_tx][MaxBeam];
	lapack_complex_float *modsymb[paraNum_tx];
	for (int i = 0; i < paraNum_tx; i++)
	{
		for (int j = 0; j < MaxBeam * cbNum; j++)
		{
			cr_data_bytes_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB / 8);
			tcod[i][j] = (srslte_tcod_t *)malloc(sizeof(srslte_tcod_t));
			srslte_tcod_init(tcod[i][j], SRSLTE_TCOD_MAX_LEN_CB);
			cr_tcod_data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * 6176 * 3);
			cr_rm_data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * max_rm_data_len_tx);
			cr_rm_data_bytes_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * max_rm_data_len_tx / 8 + 1);
			w_buff_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * 6176 * 3);
			parity_bytes[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (6148 * 2 / 8 + 1));
		}
		modsymb[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MaxBeam * CarrierNum * SymbolNum);
	}
	for (int i = 0; i < paraNum_tx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			rm_data_tx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * max_rm_data_len_tx);
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
	int cbtaskNum[paraNum_tx];
	lapack_complex_float *x[CacheNum_tx];
	for (int i = 0; i < CacheNum_tx; i++)
	{
		x[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MaxBeam * CarrierNum * SymbolNum);
	}

	lapack_complex_float *PilotSymb[MaxBeam];
	for (int i = 0; i < MaxBeam; i++)
	{
		PilotSymb[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * (i + 1) * CarrierNum * PilotSymbNum);
		CalPilotSymb(i + 1, CarrierNum, 2, 1, inter_freq, PilotSymb[i]);
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

	lapack_complex_float *channel = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * TxAntNum * CarrierNum * SymbolNum * channelNum);
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
		H[t] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * MaxBeam * CarrierNum * SymbolNum);
	}
	for (int t = 0; t < channelNum; t++)
	{
		for (int ns = 0; ns < SymbolNum; ns++)
		{
			for (int nc = 0; nc < CarrierNum; nc++)
			{
				for (int nl = 0; nl < LayerNum; nl++)
				{
					for (int nr = 0; nr < RxAntNum; nr++)
					{
						H[t][ns * CarrierNum * LayerNum * RxAntNum + nc * LayerNum * RxAntNum + nl * RxAntNum + nr] = channel[t * RxAntNum * TxAntNum * CarrierNum * SymbolNum + ns * CarrierNum * TxAntNum * RxAntNum + nc * TxAntNum * RxAntNum + H_layer[nl] * RxAntNum + nr];
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
	lapack_complex_float *gwn = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * CarrierNum * SymbolNum);
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
	lapack_complex_float *H_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * MaxBeam);
	lapack_complex_float *x_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MaxBeam * 1);
	lapack_complex_float *y_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * 1);
	lapack_complex_float *y[CacheNum_tx];
	for (int i = 0; i < CacheNum_tx; i++)
		y[i] = package_tx[i].y;

	const CBLAS_LAYOUT Layout = CblasColMajor;
	const CBLAS_TRANSPOSE transa = CblasNoTrans;
	const CBLAS_TRANSPOSE transb = CblasNoTrans;
	const MKL_INT _m = RxAntNum;
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
		for (k = 0; k < paraNum_tx; k++)
		{
			srand(seed);
			for (int i = 0; i < LayerNum; i++)
			{
				data_len_tx[k][i] = CQI_cod[CQI_index[k * MaxBeam + i]] / 1024.0 * MAX_SYMBOL_NUM * CQI_mod[CQI_index[k * MaxBeam + i]] - crc_length;
				//data_len_tx[k][i] = (rand() % (data_len_tx[k][i] / 8) + 1) * 8;
				//data_len_tx[k][i] = (rand() % 10 + 1) * 10;
				//printf("\nLayer %d : %d Original bits : ", i, data_len_tx[k][i]);
				for (int j = 0; j < data_len_tx[k][i]; j++)
				{
					data_tx[k][i][j] = rand() % 2;
					//printf("%d",data_tx[k][i][j]);
				}
			}
		}
	}
	else
		TIME_EN = 0;

	int count_ms = 0, count_s = 0, TIME = 0;

	printf("TaskScheduler tx prepared...\n");
	//--------------------Data processing--------------------
	ServiceEN_tx = (int *)malloc(sizeof(int) * paraNum_tx * taskNum_tx);
	for (int i = 0; i < paraNum_tx; i++)
	{
		for (int j = 0; j < taskNum_tx; j++)
		{
			ServiceEN_tx[i * taskNum_tx + j] = 0;
		}
	}
	for (int i = 0; i < paraNum_tx; i++)
		ServiceEN_tx[taskNum_tx * i] = 1;
	if (TIME_EN == 1)
		gettimeofday(&tx_begin, NULL); //--------------------rx

	while (1)
	{
		//sleep(1);
		//if(runIndex >= 3000) break;
		for (int n = 0; n < paraNum_tx; n++)
		{

			//  1.1  crc attach - code block segmentation
			if (startNum_tx < CacheNum_tx && ServiceEN_tx[n * taskNum_tx] == 1)
			{ //有数据需要发送，且任务添加器(n,1)打开
				pthread_mutex_lock(&mutex_startNum_tx);
				startNum_tx++;
				pthread_mutex_unlock(&mutex_startNum_tx);
				if (TIME_EN == 0)
				{
					// get new original data
					for (int i = 0; i < LayerNum; i++)
					{
						//CQI_index[n * MaxBeam + i] = rand()%15+1;
						data_len_tx[n][i] = (int)(CQI_cod[CQI_index[i]] / 1024.0 * MAX_SYMBOL_NUM * CQI_mod[CQI_index[i]] - crc_length) / 8 * 8;
						//data_len_tx[n][i] = (rand() % (data_len_tx[n][i] / 8) + 1) * 8;
						//data_len_tx[k][i] = (rand() % 10 + 1) * 10;
						//printf("\nLayer %d : %d Original bits : ", i, data_len_tx[k][i]);
						for (int j = 0; j < data_len_tx[n][i] / 8; j++)
						{
							data_bytes_tx[n][i][j] = rand() % 256;
							//printf("%d",data_tx[k][i][j]);
						}
						srslte_bit_unpack_vector(data_bytes_tx[n][i], data_tx[n][i], data_len_tx[n][i]);
					}
				}
				for (int i = 0; i < LayerNum; i++)
				{
					int j = n * MaxBeam + i;
					crc_cbsegm_args[j].crc_p = crc_tb[n][i];
					crc_cbsegm_args[j].crc_poly = crc_24A;
					crc_cbsegm_args[j].crc_length = crc_length;
					crc_cbsegm_args[j].tbs = data_len_tx[n][i];
					crc_cbsegm_args[j].tb = data_bytes_tx[n][i];
					crc_cbsegm_args[j].cb_tx = cb_tx[n][i];

					crc_cbsegm_args[j].ServiceEN = ServiceEN_tx;
					crc_cbsegm_args[j].ServiceEN_index = n * taskNum_tx + 1;

					pool_add_task(crc_cbsegm, (void *)&crc_cbsegm_args[j], 2);
				}
				ServiceEN_tx[n * taskNum_tx] = 0; //关闭任务添加器(n,1)
			}

			//  1.2  crc attach - turbo coding - rate matching - code block concatenation
			if (ServiceEN_tx[n * taskNum_tx + 1] == LayerNum)
			{ //任务添加器(n,2)打开
				int s = 0;
				cbtaskNum[n] = 0;
				for (int i = 0; i < LayerNum; i++)
				{
					cbtaskNum[n] += cb_tx[n][i]->C;
					rm_data_len_tx[n][i] = MAX_SYMBOL_NUM * CQI_mod[CQI_index[n * MaxBeam + i]];
					Lamda = MAX_SYMBOL_NUM % cb_tx[n][i]->C;
					for (int r = 0; r < (cb_tx[n][i]->C); ++r)
					{
						int j = i * cbNum + r;
						int jp = n * LayerNum * cbNum + i * cbNum + r;
						if (r < cb_tx[n][i]->C - Lamda)
							rm_outlen_tx[n][j] = CQI_mod[CQI_index[n * MaxBeam + i]] * (MAX_SYMBOL_NUM / cb_tx[n][i]->C);
						else
							rm_outlen_tx[n][j] = CQI_mod[CQI_index[n * MaxBeam + i]] * ((MAX_SYMBOL_NUM - 1) / cb_tx[n][i]->C + 1);
						SymbolBitN[n][i] = CQI_mod[CQI_index[n * MaxBeam + i]];
						cr_symnum_tx[n][j] = rm_outlen_tx[n][j] / SymbolBitN[n][i];

						crc_mod_args[jp].cbindex = r;
						crc_mod_args[jp].cb_tx = cb_tx[n][i];
						crc_mod_args[jp].tbp = data_bytes_tx[n][i];

						crc_mod_args[jp].crc_p = crc_cb[n][j];
						crc_mod_args[jp].crc_poly = crc_24B;
						crc_mod_args[jp].crc_length = crc_length;
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
						crc_mod_args[jp].max_rm_data_len_tx = max_rm_data_len_tx;

						crc_mod_args[jp].SymbolBitN = SymbolBitN[n][i];
						crc_mod_args[jp].modSymNum = cr_symnum_tx[n][j];

						crc_mod_args[jp].package = modsymb[n] + s;

						s += cr_symnum_tx[n][j];

						crc_mod_args[jp].ServiceEN = ServiceEN_tx;
						crc_mod_args[jp].ServiceEN_index = n * taskNum_tx + 2;

						pool_add_task(crc_mod, (void *)&crc_mod_args[jp], 2);
					}
				}
				ServiceEN_tx[n * taskNum_tx + 1] = 0; //关闭任务添加器(n,2)
			}

			//  1.3  packing
			if (ServiceEN_tx[n * taskNum_tx + 2] == cbtaskNum[n] && cbtaskNum[n] != 0)
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
				for (int ns = 0; ns < SymbolNum; ns++)
				{
					if ((ns != 3) && (ns != 10))
					{
						for (int nc = 0; nc < CarrierNum; nc++)
						{
							for (int i = 0; i < LayerNum; i++)
							{
								m = ns * CarrierNum * LayerNum + nc * LayerNum + i;
								if (k < MAX_SYMBOL_NUM)
								{
									x[index_write_tx][m] = modsymb[n][i * MAX_SYMBOL_NUM + k];
								}
							}
							k++;
						}
					}
					else
					{
						for (int nc = 0; nc < CarrierNum; nc++)
						{
							for (int i = 0; i < LayerNum; i++)
							{
								m = ns * CarrierNum * LayerNum + nc * LayerNum + i;
								x[index_write_tx][m] = PilotSymb[LayerNum - 1][s++];
							}
						}
					}
				}
				//printf("\ntx%d store modsymb %d ", n, index_write_tx);
				//for (int i = 0; i < LayerNum; i++){
				//	printf("%d ", data_len_tx[n][i]);
				//}
				//for (int i = 0; i < LayerNum; i++){
				//	printf("\ntx %d layer %d data : ", index_write_tx, i);
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
				for (int ns = 0; ns < SymbolNum; ns++)
				{
					for (int nc = 0; nc < CarrierNum; nc++)
					{

						for (int nl = 0; nl < LayerNum; nl++)
						{
							for (int nr = 0; nr < RxAntNum; nr++)
							{
								H_temp[nl * RxAntNum + nr] = H[t][CarrierNum * RxAntNum * LayerNum * ns + RxAntNum * LayerNum * nc + nl * RxAntNum + nr];
							}
						}
						for (int i = 0; i < LayerNum; i++)
						{
							x_temp[i] = x[index_write_tx][CarrierNum * LayerNum * ns + LayerNum * nc + i];
						}
						cblas_cgemm(Layout, transa, transb, _m, _n, _k, &alpha, H_temp, lda, x_temp, ldb, &beta, y_temp, ldc);
						for (int i = 0; i < RxAntNum; i++)
						{
							y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real = y_temp[i].real + sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real;
							y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag = y_temp[i].imag + sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag;
							//if(nc == 144) printf("\nRX %d symbol %d Carrier %d : y_temp = %f+%fi, y = %f+%fi, noise = %f+%fi\n",i,ns,nc, y_temp[i].real,y_temp[i].imag,y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real,y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag,y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real - y_temp[i].real,y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag - y_temp[i].imag);
							E += y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real * y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real + y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag * y[index_write_tx][CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag;
							Ep += y_temp[i].real * y_temp[i].real + y_temp[i].imag * y_temp[i].imag;
							Er[i] += y_temp[i].real * y_temp[i].real + y_temp[i].imag * y_temp[i].imag;
							En += sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real * sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real + sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag * sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag;
						}
					}
				}
				E /= SymbolNum * CarrierNum;
				Ep /= SymbolNum * CarrierNum;
				En /= SymbolNum * CarrierNum;
				//printf("\nSNR = %f sigma = %f E = %f Ep = %f En = %f",SNR, sigma, E ,Ep ,En);
				for (int i = 0; i < RxAntNum; i++)
				{
					Er[i] /= SymbolNum * CarrierNum;
					//printf("%f ",Er[i]);
				}
				//printf("\n");
				//printf("Through the channel...\n");
				package_tx[index_write_tx].y = y[index_write_tx];
				for (int i = 0; i < LayerNum; i++)
				{
					package_tx[index_write_tx].tbs[i] = data_len_tx[n][i];
					package_tx[index_write_tx].CQI_index[i] = CQI_index[n * MaxBeam + i];
					package_tx[index_write_tx].SNR = SNR;
					for (int j = 0; j < data_len_tx[n][i]; j++)
					{
						package_tx[index_write_tx].data[i][j] = data_tx[n][i][j];
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

				ServiceEN_tx[n * taskNum_tx + 2] = 0; //关闭任务添加器(n,3)
				cbtaskNum[n] = 0;
				//printf("\nTX task 2");
				ServiceEN_tx[n * taskNum_tx]++;
				pthread_mutex_lock(&mutex_readyNum_tx);
				readyNum_tx++;
				pthread_mutex_unlock(&mutex_readyNum_tx);
				index_write_tx++;
				if (index_write_tx == CacheNum_tx)
					index_write_tx = 0;
			}
		}

		//sleep(2);
	}
	sem_post(&tx_can_be_destroyed);
}

void TaskScheduler_rx(void *arg)
{

	//--------------------Initialize parameters--------------------
	//-----simulation-----
	struct test_rx_t *test_rx = (struct test_rx_t *)malloc(sizeof(struct test_rx_t) * paraNum_rx);
	//  1.1  Unpacking
	lapack_complex_float *package[paraNum_rx];
	for (int i = 0; i < paraNum_rx; i++)
	{
		package[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MaxBeam * CarrierNum * SymbolNum);
	}
	srslte_cbsegm_t *cb_rx[paraNum_rx][MaxBeam];
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			cb_rx[i][j] = (srslte_cbsegm_t *)malloc(sizeof(srslte_cbsegm_t));
		}
	}

	//printf("Unpacking initialized \n");
	//  1.2  channel estimating - signal detecting
	lapack_complex_float *CFR_SB[paraNum_rx][SBNum];
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < SBNum; j++)
		{
			CFR_SB[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * MaxBeam * CarrierNum * SymbolNum / SBNum);
		}
	}
	int LayerNum = LAYER_NUM;
	int inter_freq = LayerNum;
	while (CarrierNum / SBNum % inter_freq != 0)
		inter_freq++;
	struct chest_calsym_args_t *chest_calsym_args = (struct chest_calsym_args_t *)malloc(sizeof(struct chest_calsym_args_t) * SBNum * paraNum_rx);
	lapack_complex_float *y[paraNum_rx][SBNum];
	float *R_DCT[paraNum_rx][SBNum];
	lapack_complex_float *H_est[paraNum_rx][SBNum];
	lapack_complex_float *x_est[paraNum_rx][SBNum];
	lapack_complex_float *PilotSymb[MaxBeam];
	for (int i = 0; i < MaxBeam; i++)
	{
		PilotSymb[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * (i + 1) * CarrierNum * PilotSymbNum);
		CalPilotSymb(i + 1, CarrierNum, 2, 1, inter_freq, PilotSymb[i]);
	}
	lapack_complex_float *PilotSymb_SB[paraNum_rx][SBNum];
	float *SymbVar[paraNum_rx][SBNum];
	float *SINR_est[paraNum_rx][SBNum];
	int16_t *LLRD_Package[paraNum_rx];
	int *SymbolBitN_L[paraNum_rx];
	for (int i = 0; i < paraNum_rx; i++)
	{
		LLRD_Package[i] = (int16_t *)malloc(sizeof(int16_t) * MaxBeam * CarrierNum * SymbolNum * 6);
		SymbolBitN_L[i] = (int *)malloc(sizeof(int) * MaxBeam);
	}

	struct chestLS_t *chestLS_p[paraNum_rx][SBNum];
	struct chestDCT_t *chestDCT_p[paraNum_rx][SBNum];
	struct CalSymb_t *CalSymb_p[paraNum_rx][SBNum];
	struct Rest_t *Rest_p[paraNum_rx][SBNum];
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < SBNum; j++)
		{
			y[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * CarrierNum * SymbolNum / SBNum);
			R_DCT[i][j] = (float *)malloc(sizeof(float) * RxAntNum * LayerNum * CarrierNum * SymbolNum / SBNum);
			H_est[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * MaxBeam * CarrierNum * SymbolNum / SBNum);
			x_est[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MaxBeam * CarrierNum * SymbolNum / SBNum);
			PilotSymb_SB[i][j] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * MaxBeam * CarrierNum * PilotSymbNum / SBNum);
			SymbVar[i][j] = (float *)malloc(sizeof(float) * MaxBeam * CarrierNum * SymbolNum / SBNum);
			SINR_est[i][j] = (float *)malloc(sizeof(float) * MaxBeam * CarrierNum * SymbolNum / SBNum);

			chestLS_p[i][j] = (struct chestLS_t *)malloc(sizeof(struct chestLS_t));
			chestLS_init(chestLS_p[i][j], CarrierNum / SBNum, inter_freq, SymbolNum);
			chestDCT_p[i][j] = (struct chestDCT_t *)malloc(sizeof(struct chestDCT_t));
			chestDCT_init(chestDCT_p[i][j], CarrierNum / SBNum, inter_freq, SymbolNum);
			CalSymb_p[i][j] = (struct CalSymb_t *)malloc(sizeof(struct CalSymb_t));
			CalSymb_init(CalSymb_p[i][j], LayerNum, RxAntNum);
			Rest_p[i][j] = (struct Rest_t *)malloc(sizeof(struct Rest_t));
			REstimator_init(Rest_p[i][j], CarrierNum / SBNum, inter_freq, SymbolNum);
		}
	}
	int PilotSymbIndex[2];
	PilotSymbIndex[0] = 3;
	PilotSymbIndex[1] = 10;

	printf("RX task1 initialized \n");
	//  1.3  descrambling - demodulation - rate matching - turbo decoding - crc check
	int max_data_len_rx = CQI_cod[15] / 1024.0 * MAX_SYMBOL_NUM * CQI_mod[15] - crc_length;
	const int cbNum = (max_data_len_rx + crc_length) / 6144 + 1;
	uint32_t max_rm_data_len_rx = MAX_SYMBOL_NUM * CQI_mod[15];
	int max_modsymNum = MAX_SYMBOL_NUM;
	int cbtaskNum[paraNum_rx];
	int *CQI_index = (int *)malloc(sizeof(int) * MaxBeam * CacheNum_tx);

	struct derm_crc_args_t *derm_crc_args = (struct derm_crc_args_t *)malloc(sizeof(struct derm_crc_args_t) * MaxBeam * cbNum * paraNum_rx);
	int modsymNum[paraNum_rx][MaxBeam * cbNum];
	int cr_symnum_rx[paraNum_rx][MaxBeam * cbNum];
	int SymbolBitN[paraNum_rx][MaxBeam];
	uint32_t rm_outlen_rx[paraNum_rx][MaxBeam];
	uint32_t rm_data_len_rx[paraNum_rx][MaxBeam];
	uint32_t Lamda;

	genQAMtable();

	int Kr = 0;
	int16_t *cr_tdec_data_i_rx[paraNum_rx][MaxBeam * cbNum];
	srslte_tdec_t *tdec[paraNum_rx][MaxBeam * cbNum];
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < MaxBeam * cbNum; j++)
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

	uint8_t *cr_data_bytes_rx[paraNum_rx][MaxBeam * cbNum];
	srslte_crc_t *crc_cb[paraNum_rx][MaxBeam * cbNum];
	uint32_t *checksum_cb[paraNum_rx][MaxBeam];
	uint8_t *data_rx[paraNum_rx][MaxBeam];
	uint8_t *data_bytes_rx[paraNum_rx][MaxBeam];
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < MaxBeam * cbNum; j++)
		{
			crc_cb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_cb[i][j], crc_24B, crc_length);
			cr_data_bytes_rx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB / 8);
		}
	}
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			data_rx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (max_data_len_rx + crc_length));
			data_bytes_rx[i][j] = (uint8_t *)malloc(sizeof(uint8_t) * (max_data_len_rx + crc_length) / 8);
			checksum_cb[i][j] = (uint32_t *)malloc(sizeof(uint32_t) * cbNum);
			for (int k = 0; k < cbNum; k++)
			{
				checksum_cb[i][j][k] = 0;
			}
		}
	}

	//cbcon
	int data_len_rx[paraNum_rx][MaxBeam];
	printf("RX task2 initialized \n");
	//  1.4  code block desegmentation - crc check
	struct crc_check_args_t *crc_check_args = (struct crc_check_args_t *)malloc(sizeof(struct crc_check_args_t) * MaxBeam * paraNum_rx);
	srslte_crc_t *crc_tb[paraNum_rx][MaxBeam];
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < MaxBeam; j++)
		{
			crc_tb[i][j] = (srslte_crc_t *)malloc(sizeof(srslte_crc_t));
			srslte_crc_init(crc_tb[i][j], crc_24A, crc_length);
		}
	}
	uint32_t *checksum_tb[paraNum_rx];
	for (int i = 0; i < paraNum_rx; i++)
	{
		checksum_tb[i] = (uint32_t *)malloc(sizeof(uint32_t) * MaxBeam);
	}
	printf("RX task3 initialized \n");
	//-----Blocks error statistics-----
	struct BER_t *BER = (struct BER_t *)malloc(sizeof(struct BER_t));
	BER_init(BER, MaxBeam);
	int *BERsignal = (int *)malloc(sizeof(int) * paraNum_rx);
	for (int i = 0; i < paraNum_rx; i++)
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
	ServiceEN_rx = (int *)malloc(sizeof(int) * paraNum_rx * taskNum_rx);
	for (int i = 0; i < paraNum_rx; i++)
	{
		for (int j = 0; j < taskNum_rx; j++)
		{
			ServiceEN_rx[i * taskNum_rx + j] = 0;
		}
	}
	for (int i = 0; i < paraNum_rx; i++)
		ServiceEN_rx[taskNum_rx * i] = LayerNum;
	// omp_set_num_threads(1);		//?
	if (TIME_EN == 1)
		gettimeofday(&rx_begin, NULL); //--------------------rx
	while (1)
	{
		//sleep(1);
		//if(runIndex == 3000) break;
		for (int n = 0; n < paraNum_rx; n++)
		{

			//  1.2  channel estimating - signal detecting
			if (ServiceEN_rx[n * taskNum_rx] == LayerNum && readyNum_rx > 0)
			{ //有数据需要处理，且任务添加器(n,1)打开
				// if (readyNum_tx > 1)
				// 	index_read_rx = index_write_tx;
				// else
				// 	index_read_rx = 0;
				for (int i = 0; i < LayerNum; ++i)
				{
					srslte_cbsegm(cb_rx[n][i], package_rx[index_read_rx].tbs[i]);
					CQI_index[n * MaxBeam + i] = package_rx[index_read_rx].CQI_index[i];
					test_rx[n].data[i] = package_rx[index_read_rx].data[i];
					SymbolBitN_L[n][i] = CQI_mod[CQI_index[n * MaxBeam + i]];
					test_rx[n].packIdx = index_read_rx;
				}
				test_rx[n].SNR = package_rx[index_read_rx].SNR;
				for (int i = 0; i < SBNum; ++i)
				{
					int j = n * SBNum + i;
					chest_calsym_args[j].package = package_rx[index_read_rx].y;
					chest_calsym_args[j].SBindex = i;
					chest_calsym_args[j].SBNum = SBNum;

					chest_calsym_args[j].SignalRec = y[n][i];
					chest_calsym_args[j].PilotSymb_SB = PilotSymb_SB[n][i];
					chest_calsym_args[j].PilotSymb = PilotSymb[LayerNum - 1];

					chest_calsym_args[j].R_DCT = R_DCT[n][j];
					chest_calsym_args[j].PilotSymbIndex = PilotSymbIndex;
					chest_calsym_args[j].TxAntNum = TxAntNum;
					chest_calsym_args[j].RxAntNum = RxAntNum;
					chest_calsym_args[j].CarrierNum_SB = CarrierNum / SBNum;
					chest_calsym_args[j].CarrierNum = CarrierNum;
					chest_calsym_args[j].UserNum = UserNum;
					chest_calsym_args[j].MaxBeam = MaxBeam;
					chest_calsym_args[j].PilotSymbNum = PilotSymbNum;
					chest_calsym_args[j].SymbNum = SymbolNum;
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

					chest_calsym_args[j].ServiceEN_index = n * taskNum_rx + 1;
					chest_calsym_args[j].ServiceEN_rx = ServiceEN_rx;

					pool_add_task(chest_calsym, (void *)&chest_calsym_args[j], 3);
				}
				ServiceEN_rx[n * taskNum_rx] = 0;
				//printf("\nrx%d get package %d", n, index_write_tx);
				pthread_mutex_lock(&mutex_readyNum_rx);
				readyNum_rx--;
				pthread_mutex_unlock(&mutex_readyNum_rx);
				index_read_rx++;
				if (index_read_rx == CacheNum_rx)
					index_read_rx = 0;
			}

			//  1.3  descrambling - demodulation - rate matching - turbo decoding - crc check
			if (ServiceEN_rx[n * taskNum_rx + 1] == SBNum)
			{

				//printf("\nrx%d x_est : ",n);
				//for(int i = 0; i < 8; i++) printf("%f+%fi ",x_est[n][0][i]);

				int s = 0;
				cbtaskNum[n] = 0;
				for (int i = 0; i < LayerNum; i++)
				{
					int c = 0;
					rm_data_len_rx[n][i] = MAX_SYMBOL_NUM * CQI_mod[CQI_index[n * MaxBeam + i]];
					Lamda = MAX_SYMBOL_NUM % cb_rx[n][i]->C;
					cbtaskNum[n] += cb_rx[n][i]->C;
					for (int r = 0; r < (cb_rx[n][i]->C); ++r)
					{
						int j = i * cbNum + r;
						int jp = n * LayerNum * cbNum + i * cbNum + r;
						if (r < cb_rx[n][i]->C - Lamda)
							rm_outlen_rx[n][i] = CQI_mod[CQI_index[n * MaxBeam + i]] * (MAX_SYMBOL_NUM / cb_rx[n][i]->C);
						else
							rm_outlen_rx[n][i] = CQI_mod[CQI_index[n * MaxBeam + i]] * ((MAX_SYMBOL_NUM - 1) / cb_rx[n][i]->C + 1);
						Kr = (r < cb_rx[n][i]->C2) ? cb_rx[n][i]->K2 : cb_rx[n][i]->K1;
						SymbolBitN[n][i] = CQI_mod[CQI_index[n * MaxBeam + i]];
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
						derm_crc_args[jp].crc_poly = crc_24B;
						derm_crc_args[jp].crc_length = crc_length;
						derm_crc_args[jp].checksum = checksum_cb[n][i];
						derm_crc_args[jp].r = r;
						derm_crc_args[jp].cbnum = cb_rx[n][i]->C;

						derm_crc_args[jp].tb = data_bytes_rx[n][i];
						derm_crc_args[jp].conindex = c;
						derm_crc_args[jp].F = cb_rx[n][i]->F;
						if ((cb_rx[n][i]->C) > 1)
						{
							if (r == 0)
								c += Kr - cb_rx[n][i]->F - crc_length;
							else
								c += Kr - crc_length;
						}

						derm_crc_args[jp].ServiceEN_index = n * taskNum_rx + 2;
						derm_crc_args[jp].ServiceEN_rx = ServiceEN_rx;

						pool_add_task(derm_crc, (void *)&derm_crc_args[jp], 3);
					}
				}
				ServiceEN_rx[n * taskNum_rx + 1] = 0;
			}
			//printf("\n%d = %d?",ServiceEN_rx[n * taskNum_rx + 2],cbtaskNum[n]);

			//  1.4  code block desegmentation - crc check
			if (ServiceEN_rx[n * taskNum_rx + 2] == cbtaskNum[n] && cbtaskNum[n] != 0)
			{

				for (int i = 0; i < LayerNum; i++)
				{
					int j = n * LayerNum + i;
					data_len_rx[n][i] = cb_rx[n][i]->tbs;
					//printf("\nLayer %d data length : %d", i, data_len_rx[n][i]);
					//printf("\nrx %d layer %d data : ", n, i);
					//for(int k = 0; k < 10; k++) printf("%d", data_rx[n][i][k]);
					crc_check_args[j].crc_p = crc_tb[n][i];
					crc_check_args[j].crc_poly = crc_24A;
					crc_check_args[j].crc_length = crc_length;
					crc_check_args[j].tb = data_bytes_rx[n][i];
					crc_check_args[j].checksum = checksum_tb[n];
					crc_check_args[j].layer = i;
					crc_check_args[j].tbs = data_len_rx[n][i];

					crc_check_args[j].subframeIndex = n;
					crc_check_args[j].BERsignal = BERsignal;

					pool_add_task(crc_check, (void *)&crc_check_args[j], 3);
				}
				ServiceEN_rx[n * taskNum_rx + 2] = 0;
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
				ServiceEN_rx[n * taskNum_rx] = LayerNum;
			}
		}
	}
	sem_post(&rx_can_be_destroyed);
	freeQAMtable();
}
