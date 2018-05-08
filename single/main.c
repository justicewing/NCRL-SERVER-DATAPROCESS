#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <complex.h>
#include <sys/time.h>
#include "mkl.h"
#include "omp.h"

#include "srslte/fec/crc.h"
#include "srslte/fec/turbodecoder.h"
#include "srslte/fec/cbsegm.h"
#include "srslte/utils/bit.h"
#include "srslte/fec/turbocoder.h"
#include "srslte/fec/rm_turbo.h"
#include "srslte/utils/vector.h"
#include "srslte/config.h"

#include "ChannelEstimator.h"
#include "CalSymb_mmse.h"
#include "RxAdptLink.h"
#include "NQAMMod.h"

#include "config.h"

#define BER_FILE "statistics/BER_detect_flow5.txt"
#define FER_FILE "statistics/FER_detect_flow5.txt"
#define SNR_FILE "statistics/SNR_detect_flow5.txt"

int main()
{
	/*==========running time test==========*/

	struct timeval crc_begin, crc_end;
	struct timeval cbsegm_begin, cbsegm_end;
	struct timeval tcod_begin, tcod_end;
	struct timeval rm_begin, rm_end;
	struct timeval mod_begin, mod_end;
	struct timeval pack_begin, pack_end;

	struct timeval chest_begin, chest_end;
	struct timeval sigdect_begin, sigdect_end;
	struct timeval cqi_begin, cqi_end;
	struct timeval demod_begin, demod_end;
	struct timeval derm_begin, derm_end;
	struct timeval tdec_begin, tdec_end;
	struct timeval crccheck_begin, crccheck_end;

	struct timeval tx_begin, tx_end;
	struct timeval rx_begin, rx_end;

	struct RunTime runtime;

	runtime.crc = 0;
	runtime.cbsegm = 0;
	runtime.tcod = 0;
	runtime.rm = 0;
	runtime.mod = 0;
	runtime.pack = 0;
	runtime.others_tx = 0;

	runtime.noiseest = 0;
	runtime.chest = 0;
	runtime.sigdect = 0;
	runtime.cqi = 0;
	runtime.demod = 0;
	runtime.derm = 0;
	runtime.tdec = 0;
	runtime.crccheck = 0;
	runtime.others_rx = 0;

	runtime.tx = 0;
	runtime.rx = 0;

	/*==========running time test==========*/
	/*----------测试参数设置----------*/
	omp_set_num_threads(1);
	uint32_t seed = 1;
	int layerNum = 4; // 流数
	const int SNR_min = 0, SNR_max = 50;
	const int CQI_min = 15, CQI_max = 15;
	const int loopNum = 10; // 循环次数;num_block = loopNum * floorNum
	const int step = 1;
	const int datasymNum = 12;
	const int subframeNum = 1;
	int H_Type = 1;			//0：理想信道  1：估计信道	2:理想信道导频部分
	int ChEstType = 0;		//0：LS  1：DCT
	int LinkAdptState = 0;	//0: 固定CQI 1：链路自适应

	int *CQI_index = (int *)malloc(sizeof(int) * MaxBeam);
	int *CQI_index_ = (int *)malloc(sizeof(int) * LayerNum);
	//for(int i = 0; i < MaxBeam; i++) CQI_index[i] = 1;
	//printf("\ntest CQI = %d\n",CQI_index[0]);

	//int H_layer[8] = {91,90,93,92,89,88,87,95};//1
	//int H_layer[8] = {87,88,89,90,91,92,93};
	int H_layer[8] = {76, 77, 75, 73, 74, 72, 78, 79}; //2
	//int H_layer[8] = {72,73,74,75,76,77,78,79};
	//int H_layer[8] = {66,67,68,65,69,64,70,63};//3
	//int H_layer[8] = {63,64,65,66,67,68,69,70};

	int inter_freq = layerNum;
	while (CarrierNum % inter_freq != 0)
		inter_freq++;
	// printf("double:%d float:%d int16_t:%d\n",
	// 	   sizeof(double), sizeof(float), sizeof(int16_t));

	/*----------读取信道信息----------*/
	//信道矩阵
	printf("Start Setting Channel...\n");
	uint32_t m = 0;
	lapack_complex_float *channel =
		(lapack_complex_float *)malloc(sizeof(lapack_complex_float) *
									   RxAntNum * LayerNum * CarrierNum *
									   symbolNum * subframeNum);
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
	printf("Set Channel Down!\n");

	lapack_complex_float *H[subframeNum];
	for (int t = 0; t < subframeNum; t++)
	{
		H[t] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * LayerNum * CarrierNum * symbolNum);
	}
	for (int t = 0; t < subframeNum; t++)
	{
		for (int ns = 0; ns < symbolNum; ns++)
		{
			for (int nc = 0; nc < CarrierNum; nc++)
			{
				for (int nl = 0; nl < layerNum; nl++)
				{
					for (int nr = 0; nr < RxAntNum; nr++)
					{
						H[t][ns * CarrierNum * layerNum * RxAntNum +
							 nc * layerNum * RxAntNum + nl * RxAntNum + nr] =
							channel[t * RxAntNum * LayerNum * CarrierNum * symbolNum +
									ns * CarrierNum * LayerNum * RxAntNum + nc * LayerNum * RxAntNum +
									H_layer[nl] * RxAntNum + nr];
					}
				}
			}
		}
	}
	free(channel);

	//信道相关阵
	float *R_DCT_all = (float *)malloc(sizeof(float) * RxAntNum * LayerNum * CarrierNum * symbolNum * subframeNum);
	float *R_DCT_ = (float *)malloc(sizeof(float) * RxAntNum * LayerNum * CarrierNum * symbolNum);
	FILE *fp_R;
	fp_R = fopen("channel/R_DCT_1200_140.txt", "r");
	m = 0;
	if (!fp_R)
		printf("Can not open the file.\n");
	while (!feof(fp_R))
	{
		fscanf(fp_R, "%f", &R_DCT_all[m]);
		//if(m < 10) printf("%d %f\n",m,R_DCT_all[m]);
		m++;
	}
	fclose(fp_R);
	//printf("m = %d\n",m);
	float *R_DCT[subframeNum];
	for (int t = 0; t < subframeNum; t++)
	{
		R_DCT[t] = (float *)malloc(sizeof(float) * RxAntNum * LayerNum * CarrierNum * symbolNum);
	}
	for (int t = 0; t < subframeNum; t++)
	{
		for (int ns = 0; ns < symbolNum; ns++)
		{
			for (int nc = 0; nc < CarrierNum; nc++)
			{
				for (int nl = 0; nl < layerNum; nl++)
				{
					for (int nr = 0; nr < RxAntNum; nr++)
					{
						R_DCT[t][ns * CarrierNum * layerNum * RxAntNum +
								 nc * layerNum * RxAntNum + nl * RxAntNum + nr] =
							R_DCT_all[t * RxAntNum * layerNum * CarrierNum * symbolNum +
									  ns * CarrierNum * layerNum * RxAntNum +
									  nc * layerNum * RxAntNum +
									  nl * RxAntNum + nr];
					}
				}
			}
		}
		//printf("%d",t);
	}
	//printf("test");
	free(R_DCT_all);

	//高斯白噪声
	FILE *fp_nr;
	fp_nr = fopen("channel/wgnr.txt", "r");
	FILE *fp_ni;
	fp_ni = fopen("channel/wgni.txt", "r");
	lapack_complex_float *gwn =
		(lapack_complex_float *)malloc(sizeof(lapack_complex_float) *
									   RxAntNum * CarrierNum * symbolNum);
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
	//printf("Channel loading is finished...\n");

	/*----------初始化----------*/
	//1.0  生成数据
	int data_len_tx[layerNum]; /// layerNum?
	uint8_t *data_tx[layerNum];
	uint8_t *data_bytes_tx[layerNum];
	int max_symbol_num = CarrierNum * datasymNum;
	int max_data_len_tx =
		(CQI_cod[15] /
			 1024.0 * max_symbol_num * CQI_mod[15] -
		 crc_length) /
		8 * 8;
	for (int i = 0; i < layerNum; i++)
	{
		data_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) *
								  max_data_len_tx +
							  crc_length);
		data_bytes_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) *
							  (max_data_len_tx + crc_length) / 8);
	}
	//1.1  CRC添加(data_tx -> data_tx)
	srslte_crc_t crc_p;
	//1.2  码块分割
	srslte_cbsegm_t cb_tx[layerNum];
	//1.3  对每个码块进行CRC添加、Turbo编码、速率匹配，最后码块级联
	//(data_tx -> cr_data_tx -> cr_tcod_data_tx -> cr_rm_data_tx -> rm_data_tx)
	uint8_t *cr_data_tx[layerNum];
	uint8_t *cr_data_bytes_tx[layerNum];
	uint8_t *cr_tcod_data_tx[layerNum];
	srslte_tcod_t tcod;
	srslte_tcod_init(&tcod, SRSLTE_TCOD_MAX_LEN_CB);
	uint8_t *cr_rm_data_tx[layerNum];
	uint8_t *cr_rm_data_bytes_tx[layerNum];
	uint8_t *w_buff_tx[layerNum];
	uint8_t *parity_bytes =
		(uint8_t *)malloc(sizeof(uint8_t) * (6148 * 2 / 8 + 1));
	uint32_t max_rm_data_len_tx = max_symbol_num * CQI_mod[15]; //?
	uint8_t *rm_data_tx[layerNum];
	uint32_t rm_data_len_tx[layerNum];
	uint32_t rm_outlen_tx[layerNum];
	uint32_t Kr, k, s, j;
	uint32_t Lamda;
	for (int i = 0; i < layerNum; i++)
	{
		cr_data_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB);
		cr_data_bytes_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB / 8);
		cr_tcod_data_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * 6176 * 3);
		cr_rm_data_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * max_rm_data_len_tx);
		cr_rm_data_bytes_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * max_rm_data_len_tx / 8 + 1);
		w_buff_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * 6176 * 3);
		rm_data_tx[i] =
			(uint8_t *)malloc(sizeof(uint8_t) * max_rm_data_len_tx);
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
	srslte_rm_turbo_gentables();
	srslte_tcod_gentable();
	//1.4  加扰
	//1.5  符号映射(rm_data_tx -> mod_data_tx)
	int SymbolBitN[layerNum];
	int mod_data_len_tx[layerNum];
	lapack_complex_float *mod_data_tx[layerNum];
	int max_mod_data_len_tx = max_rm_data_len_tx / CQI_mod[1];
	for (int i = 0; i < layerNum; i++)
	{
		mod_data_tx[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * max_mod_data_len_tx);
	}
	//1.5  组包(mod_data_tx -> x)
	lapack_complex_float *PilotSymb = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * LayerNum * CarrierNum * 2);
	CalPilotSymb(layerNum, CarrierNum, 2, 1, inter_freq, PilotSymb);
	lapack_complex_float c_zero;
	c_zero.real = 0;
	c_zero.imag = 0;
	lapack_complex_float *x = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * LayerNum * CarrierNum * symbolNum);
	//1.6  通过信道
	lapack_complex_float *H_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * LayerNum);
	lapack_complex_float *x_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * LayerNum * 1);
	lapack_complex_float *y_temp = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * 1);
	lapack_complex_float *y = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * CarrierNum * symbolNum);

	const CBLAS_LAYOUT Layout = CblasColMajor;
	const CBLAS_TRANSPOSE transa = CblasNoTrans;
	const CBLAS_TRANSPOSE transb = CblasNoTrans;
	const MKL_INT _m = RxAntNum;
	const MKL_INT _n = 1;
	const MKL_INT _k = layerNum;
	lapack_complex_float alpha;
	alpha.real = 1;
	alpha.imag = 0;
	lapack_complex_float beta;
	beta.real = 0;
	beta.imag = 0;
	const MKL_INT lda = _m;
	const MKL_INT ldb = _k;
	const MKL_INT ldc = _m;

	//2.1  解包(package_data_rx -> signal_data_rx)

	//2.2  信道估计
	lapack_complex_float *H_est = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * RxAntNum * LayerNum * CarrierNum * symbolNum);
	int PilotSymbIndex[2];
	PilotSymbIndex[0] = 3;
	PilotSymbIndex[1] = 10;
	float noise2;
	//2.3  信号检测
	lapack_complex_float *x_est = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * LayerNum * CarrierNum * symbolNum);
	float *SymbVar = (float *)malloc(sizeof(float) * LayerNum * CarrierNum * symbolNum);
	float *SINR_est = (float *)malloc(sizeof(float) * LayerNum * CarrierNum * symbolNum);
	float SymbVarM[MaxBeam];
	float SINR[MaxBeam];
	genAdptLinktable();
	//2.4  解符号映射
	int mod_data_len_rx[layerNum];
	lapack_complex_float *mod_data_rx[layerNum];
	int max_mod_data_len_rx = max_rm_data_len_tx / CQI_mod[1];
	float *Sigma2[layerNum];
	float *demod_LLRD[layerNum];
	float *demod_LLRA[layerNum];
	for (int i = 0; i < layerNum; i++)
	{
		mod_data_rx[i] = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * max_mod_data_len_rx);
		Sigma2[i] = (float *)malloc(sizeof(float) * max_mod_data_len_rx);
		demod_LLRA[i] = (float *)malloc(sizeof(float) * max_mod_data_len_rx * CQI_mod[15]);
		demod_LLRD[i] = (float *)malloc(sizeof(float) * max_mod_data_len_rx * CQI_mod[15]);
	}
	genQAMtable();
	//2.5  解扰

	//2.6  解码块级联(demod_LLRD -> rm_data_rx)
	srslte_cbsegm_t cb_rx[layerNum];
	float *rm_data_rx[layerNum];
	uint32_t rm_data_len_rx[layerNum];
	uint32_t max_rm_data_len_rx = max_rm_data_len_tx;
	for (int i = 0; i < layerNum; i++)
	{
		rm_data_rx[i] = (float *)malloc(sizeof(float) * max_rm_data_len_rx);
	}
	//2.7  对每个码块进行解速率匹配、Turbo译码、CRC校验、最后解码块分割(rm_data_rx -> cr_rm_data_rx -> cr_tcod_data_rx -> cr_data_rx -> data_rx)
	float *cr_rm_data_rx[layerNum];
	int16_t *cr_rm_datai_rx[layerNum];
	uint32_t cr_rm_data_len_rx[layerNum];
	float *w_buff_rx[layerNum];
	int16_t *cr_tdec_data_i_rx[layerNum];
	float *cr_tdec_data_f_rx[layerNum];
	uint32_t cr_tdec_data_len_rx[layerNum];
	uint8_t *cr_data_rx[layerNum];
	uint8_t *cr_data_bytes_rx[layerNum];
	uint8_t *data_rx[layerNum];
	uint8_t *data_bytes_rx[layerNum];
	uint32_t data_len_rx[layerNum];
	uint32_t cr_checksum[layerNum];
	srslte_tdec_t tdec;
	srslte_tdec_init(&tdec, 6144);
	for (int i = 0; i < layerNum; ++i)
	{
		cr_rm_data_rx[i] = (float *)malloc(sizeof(float) * max_rm_data_len_rx);
		cr_rm_datai_rx[i] = (int16_t *)malloc(sizeof(int16_t) * max_rm_data_len_rx);
		w_buff_rx[i] = (float *)malloc(sizeof(float) * 6176 * 3);
		cr_tdec_data_i_rx[i] = (int16_t *)malloc(sizeof(int16_t) * 6176 * 3);
		cr_tdec_data_f_rx[i] = (float *)malloc(sizeof(float) * 6176 * 3);
		cr_data_rx[i] = (uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB);
		cr_data_bytes_rx[i] = (uint8_t *)malloc(sizeof(uint8_t) * SRSLTE_TCOD_MAX_LEN_CB / 8);
		data_rx[i] = (uint8_t *)malloc(sizeof(uint8_t) * CarrierNum * symbolNum * RxAntNum);
		data_bytes_rx[i] = (uint8_t *)malloc(sizeof(uint8_t) * CarrierNum * symbolNum * RxAntNum / 8);
	}
	//2.8  CRC校验
	uint32_t checksum[layerNum];
	uint32_t error_num = 0, error_num_l[LayerNum], data_num = 0;
	/*----------误码率、误块率统计信息----------*/
	float errors = 0.0;
	int index = 0;
	float SNR = 1;
	float sigma = 0.0;
	float block_errors = 0.0;
	int block_error_all = 0, block_all = 0;
	uint32_t error_all = 0, bits_all = 0;
	FILE *fp_er;
	fp_er = fopen(BER_FILE, "w+");
	FILE *fp_ber;
	fp_ber = fopen(FER_FILE, "w+");
	FILE *fp_sg;
	fp_sg = fopen(SNR_FILE, "w+");
	printf("start running...\n");

	int layer_check[MaxBeam];
	for (int c = CQI_min; c <= (LinkAdptState == 1 ? CQI_min : CQI_max); c++)
	{
		for (int i = 0; i < MaxBeam; i++)
			CQI_index[i] = c;

		for (int SNR_test = SNR_min; SNR_test <= SNR_max; SNR_test++)
		{
			for (int step_test = 0; step_test < step; step_test++)
			{
				block_error_all = 0;
				error_all = 0;
				bits_all = 0;
				block_all = 0;
				//seed = time(NULL);
				srand(seed);
				runtime.crc = 0;
				runtime.cbsegm = 0;
				runtime.tcod = 0;
				runtime.rm = 0;
				runtime.mod = 0;
				runtime.pack = 0;

				runtime.chest = 0;
				runtime.sigdect = 0;
				runtime.cqi = 0;
				runtime.demod = 0;
				runtime.derm = 0;
				runtime.tdec = 0;
				runtime.crccheck = 0;

				runtime.tx = 0;
				runtime.rx = 0;

				bzero(layer_check, MaxBeam * sizeof(int));
				if (LinkAdptState == 1)
					for (int i = 0; i < MaxBeam; i++)
						CQI_index[i] = 1;
				for (int w = 0; w < loopNum; w++)
				{
					for (int t = 0; t < subframeNum; t++)
					{
						/*----------发送端----------*/
						//1.0  生成数据
						block_all += layerNum;
						srand(seed);
						for (int i = 0; i < layerNum; i++)
						{
							data_len_tx[i] =
								(int)(CQI_cod[CQI_index[i]] /
										  1024.0 * max_symbol_num * CQI_mod[CQI_index[i]] -
									  crc_length) /
								8 * 8;
							//data_len_tx[i] = data_len_tx[i] / 8;
							//data_len_tx[i] = (rand() % (data_len_tx[i] / 8) + 1) * 8;
							//data_len_tx[i] = 16;
							bits_all += data_len_tx[i];
						}
						for (int i = 0; i < layerNum; i++)
						{
							//printf("\n%d Original bits in Layer %d :",data_len_tx[i], i);
							for (int j = 0; j < data_len_tx[i] / 8; j++)
							{
								data_bytes_tx[i][j] = rand() % 256;
								//printf("%d",data_tx[i][j]);
							}
							srslte_bit_unpack_vector(data_bytes_tx[i], data_tx[i], data_len_tx[i]);
						}
						gettimeofday(&tx_begin, NULL);
						//1.1  CRC添加(data_tx -> data_tx)
						gettimeofday(&crc_begin, NULL); //--------------------crc

						srslte_crc_init(&crc_p, crc_24A, crc_length);
						for (int i = 0; i < layerNum; i++)
						{
							//srslte_crc_attach(&crc_p, data_tx[i], data_len_tx[i]);
							srslte_crc_attach_byte(&crc_p, data_bytes_tx[i], data_len_tx[i]);
							//printf("\n%d CRC added bits in layer %d:", data_len_tx[i] + crc_length, i);
							//for (int j = 0; j < data_len_tx[i] + crc_length; j++) printf("%d",data_tx[i][j]);
						}

						gettimeofday(&crc_end, NULL); //______________________
						runtime.crc += (crc_end.tv_sec - crc_begin.tv_sec) + (crc_end.tv_usec - crc_begin.tv_usec) / 1000000.0;

						//1.2  码块分割
						gettimeofday(&cbsegm_begin, NULL); //--------------------cbsegm

						for (int i = 0; i < layerNum; i++)
						{
							srslte_cbsegm(&cb_tx[i], data_len_tx[i]);
							//printf("\nCB Segmentation in layer %d: TBS=%d, C=%d, C+=%d K+=%d, C-=%d, K-=%d, F=%d", i, cb_tx[i].tbs, cb_tx[i].C, cb_tx[i].C1, cb_tx[i].K1, cb_tx[i].C2, cb_tx[i].K2, cb_tx[i].F);
						}

						gettimeofday(&cbsegm_end, NULL); //______________________
						runtime.cbsegm += (cbsegm_end.tv_sec - cbsegm_begin.tv_sec) + (cbsegm_end.tv_usec - cbsegm_begin.tv_usec) / 1000000.0;

						//1.3  对每个码块进行CRC添加、Turbo编码、速率匹配，最后码块级联(data_tx -> cr_data_tx -> cr_tcod_data_tx -> cr_rm_data_tx -> rm_data_tx)
						srslte_crc_init(&crc_p, crc_24B, crc_length);
						for (int i = 0; i < layerNum; i++)
						{
							s = 0;
							j = 0;
							rm_data_len_tx[i] = max_symbol_num * CQI_mod[CQI_index[i]];
							Lamda = max_symbol_num % cb_tx[i].C;
							for (int r = 0; r < cb_tx[i].C; ++r)
							{
								k = 0;
								if (r < cb_tx[i].C - Lamda)
									rm_outlen_tx[i] = CQI_mod[CQI_index[i]] * (max_symbol_num / cb_tx[i].C);
								else
									rm_outlen_tx[i] = CQI_mod[CQI_index[i]] * ((max_symbol_num - 1) / cb_tx[i].C + 1);
								//printf("cr_rm_data_len_tx :%d\n",rm_outlen_tx[i]);
								if (r == 0)
									for (k = 0; k < cb_tx[i].F / 8; ++k)
										cr_data_bytes_tx[i][k] = 0;
								Kr = (r < cb_tx[i].C2) ? cb_tx[i].K2 : cb_tx[i].K1;
								while (k < (Kr - crc_length) / 8)
								{
									cr_data_bytes_tx[i][k] = data_bytes_tx[i][s];
									++s;
									++k;
								}
								if (cb_tx[i].C > 1)
								{

									gettimeofday(&crc_begin, NULL); //--------------------crc

									//srslte_crc_attach(&crc_p, cr_data_tx[i], Kr - crc_length);
									srslte_crc_attach_byte(&crc_p, cr_data_bytes_tx[i], Kr - crc_length);

									gettimeofday(&crc_end, NULL); //______________________
									runtime.crc += (crc_end.tv_sec - crc_begin.tv_sec) + (crc_end.tv_usec - crc_begin.tv_usec) / 1000000.0;
								}
								else
								{
									while (k < Kr / 8)
									{
										cr_data_bytes_tx[i][k] = data_bytes_tx[i][s];
										++s;
										++k;
									}
								}
								//printf("\nlayer %d CB %d : %d CRC added bits:", i, r, Kr);
								//for (int n = 0; n < 80; ++n) printf("%d",cr_data_tx[i][n]);

								/*
			gettimeofday(&tcod_begin, NULL);//--------------------tcod
			srslte_tcod_encode(&tcod, cr_data_tx[i], cr_tcod_data_tx[i], Kr);
			//printf("\nlayer %d CB %d : %d turbocode bits:", i, r, Kr * 3 + TOTALTAIL);
			//for (int n = 0; n < 80; ++n) printf("%d",cr_tcod_data_tx[i][n]);
			gettimeofday(&tcod_end, NULL);//______________________
			runtime.tcod += (tcod_end.tv_sec - tcod_begin.tv_sec) + (tcod_end.tv_usec - tcod_begin.tv_usec) / 1000000.0;

			gettimeofday(&rm_begin, NULL);//--------------------rm
			bzero(w_buff_tx[i],sizeof(uint8_t) * 6176 * 3);
			srslte_rm_turbo_tx(w_buff_tx[i], BUFFSZ, cr_tcod_data_tx[i], Kr * 3 + TOTALTAIL, cr_rm_data_tx[i], rm_outlen_tx[i], 0);
			gettimeofday(&rm_end, NULL);//______________________
			runtime.rm += (rm_end.tv_sec - rm_begin.tv_sec) + (rm_end.tv_usec - rm_begin.tv_usec) / 1000000.0;
//*/
								//*lut
								gettimeofday(&tcod_begin, NULL); //--------------------tcod
								srslte_tcod_encode_lut(&tcod, cr_data_bytes_tx[i], parity_bytes, detc_cb_sizes[Kr]);
								gettimeofday(&tcod_end, NULL); //______________________
								runtime.tcod += (tcod_end.tv_sec - tcod_begin.tv_sec) + (tcod_end.tv_usec - tcod_begin.tv_usec) / 1000000.0;

								gettimeofday(&rm_begin, NULL); //--------------------rm
								bzero(w_buff_tx[i], 6176 * 3 * sizeof(uint8_t));
								bzero(cr_rm_data_bytes_tx[i], (max_rm_data_len_tx / 8 + 1) * sizeof(uint8_t));
								srslte_rm_turbo_tx_lut(w_buff_tx[i], cr_data_bytes_tx[i], parity_bytes, cr_rm_data_bytes_tx[i], detc_cb_sizes[Kr], rm_outlen_tx[i], 0, 0);
								srslte_bit_unpack_vector(cr_rm_data_bytes_tx[i], cr_rm_data_tx[i], rm_outlen_tx[i]);
								gettimeofday(&rm_end, NULL); //______________________
								runtime.rm += (rm_end.tv_sec - rm_begin.tv_sec) + (rm_end.tv_usec - rm_begin.tv_usec) / 1000000.0;
								//*/

								//printf("\nlayer %d CB %d : %d rm bits:", i, r, rm_outlen_tx[i]);
								//for (int n = 0; n < 80; ++n) printf("%d",cr_rm_data_tx[i][n]);
								for (int n = 0; n < rm_outlen_tx[i]; ++n)
								{
									//printf("%d",cr_rm_data_tx[i][n]);
									rm_data_tx[i][j] = cr_rm_data_tx[i][n];
									j++;
								}
							}
							//printf("\n%d CB concatenation bits in layer %d:", rm_data_len_tx[i], i);
							//for (int n = 0; n < rm_data_len_tx[i]; ++n) printf("%d",rm_data_tx[i][n]);
						}
						//1.4  加扰
						//1.5  符号映射(rm_data_tx -> mod_data_tx)

						for (int i = 0; i < layerNum; i++)
						{
							SymbolBitN[i] = CQI_mod[CQI_index[i]];
							//printf("\nLayer %d SymbolBitN:%d",i,SymbolBitN[i]);
							mod_data_len_tx[i] = rm_data_len_tx[i] / SymbolBitN[i];

							gettimeofday(&mod_begin, NULL); //--------------------mod

							QAM_Modulation(rm_data_tx[i], mod_data_tx[i], SymbolBitN[i], mod_data_len_tx[i]);
							//printf("\n%d modulation symblos in layer %d:", mod_data_len_tx[i], i);
							//for (int n = 0; n < 20; ++n) printf("%f+%fi ",mod_data_tx[i][n].real,mod_data_tx[i][n].imag);

							gettimeofday(&mod_end, NULL); //______________________
							runtime.mod += (mod_end.tv_sec - mod_begin.tv_sec) + (mod_end.tv_usec - mod_begin.tv_usec) / 1000000.0;
						}
						//1.5  组包(mod_data_tx -> x)
						gettimeofday(&pack_begin, NULL); //--------------------pack
						j = 0;
						k = 0;
						s = 0;
						for (int ns = 0; ns < symbolNum; ns++)
						{
							if ((ns != 3) && (ns != 10))
							{
								for (int nc = 0; nc < CarrierNum; nc++)
								{
									for (int i = 0; i < layerNum; i++)
									{
										j = ns * CarrierNum * layerNum + nc * layerNum + i;
										x[j] = mod_data_tx[i][k];
									}
									k++;
								}
							}
							else
							{
								for (int nc = 0; nc < CarrierNum; nc++)
								{
									for (int i = 0; i < layerNum; i++)
									{
										j = ns * CarrierNum * layerNum + nc * layerNum + i;
										x[j] = PilotSymb[s++];
										//printf("%d ",j);
									}
								}
							}
						}
						gettimeofday(&pack_end, NULL); //______________________
						runtime.pack += (pack_end.tv_sec - pack_begin.tv_sec) + (pack_end.tv_usec - pack_begin.tv_usec) / 1000000.0;

						//printf("\nTX is finished...\n");

						gettimeofday(&tx_end, NULL); //______________________
						runtime.tx += (tx_end.tv_sec - tx_begin.tv_sec) + (tx_end.tv_usec - tx_begin.tv_usec) / 1000000.0;

						/*----------通过信道----------*/
						SNR = SNR_test + step_test * 1.0 / step;
						sigma = sqrt(1 / pow(10, SNR / 10));
						//printf("SNR : %f sigma : %f",SNR,sigma);
						float E = 0, Ep = 0, En = 0;
						float Er[8];
						bzero(Er, sizeof(float) * 8);

						for (int ns = 0; ns < symbolNum; ns++)
						{
							for (int nc = 0; nc < CarrierNum; nc++)
							{

								for (int nl = 0; nl < layerNum; nl++)
								{
									for (int nr = 0; nr < RxAntNum; nr++)
									{
										H_temp[nl * RxAntNum + nr] = H[t][CarrierNum * RxAntNum * layerNum * ns + RxAntNum * layerNum * nc + nl * RxAntNum + nr];
									}
								}
								for (int i = 0; i < layerNum; i++)
								{
									x_temp[i] = x[CarrierNum * layerNum * ns + layerNum * nc + i];
								}
								//bzero(y_temp,sizeof(lapack_complex_float) * RxAntNum);
								cblas_cgemm(Layout, transa, transb, _m, _n, _k, &alpha, H_temp, lda, x_temp, ldb, &beta, y_temp, ldc);
								for (int i = 0; i < RxAntNum; i++)
								{
									y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real = y_temp[i].real + sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real;
									y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag = y_temp[i].imag + sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag;
									//if(nc == 144) printf("\nRX %d symbol %d Carrier %d : y_temp = %f+%fi, y = %f+%fi, noise = %f+%fi\n",i,ns,nc, y_temp[i].real,y_temp[i].imag,y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real,y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag,y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real - y_temp[i].real,y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag - y_temp[i].imag);
									E += y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real * y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real + y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag * y[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag;
									Ep += y_temp[i].real * y_temp[i].real + y_temp[i].imag * y_temp[i].imag;
									Er[i] += y_temp[i].real * y_temp[i].real + y_temp[i].imag * y_temp[i].imag;
									En += sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real * sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].real + sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag * sigma * gwn[CarrierNum * RxAntNum * ns + RxAntNum * nc + i].imag;
								}
							}
						}
						E /= symbolNum * CarrierNum;
						Ep /= symbolNum * CarrierNum;
						En /= symbolNum * CarrierNum;
						//printf("%f %f %f\n",E,Ep,En);
						for (int i = 0; i < RxAntNum; i++)
						{
							Er[i] /= symbolNum * CarrierNum;
							//printf("%f ",Er[i]);
						}
						//printf("\n");
						//printf("Through the channel...\n");
						/*----------接收端----------*/
						gettimeofday(&rx_begin, NULL);
						//2.1  解包(package_data_rx -> signal_data_rx)

						//2.2  信道估计
						gettimeofday(&chest_begin, NULL); //--------------------chest
						if (ChEstType == 1)
						{
							noise2 = REstimator(y, PilotSymb, PilotSymbIndex, layerNum, RxAntNum, CarrierNum, symbolNum, 2, inter_freq, R_DCT_);
							//noise2 = NoiseEstimator(y, PilotSymb, PilotSymbIndex, layerNum, RxAntNum, CarrierNum, 1, 2, inter_freq);
							//printf("sigma2 = %lf , noise2 = %lf\n", sigma *sigma, noise2);
							ChannelEstimator_DCT(y, PilotSymb, R_DCT_, PilotSymbIndex, layerNum, RxAntNum, CarrierNum, 1, layerNum, 2, symbolNum, inter_freq, sqrt(noise2), H_est);
						}
						else
						{
							noise2 = ChannelEstimator_LS(y, PilotSymb, PilotSymbIndex, layerNum, RxAntNum, CarrierNum, 2, symbolNum, inter_freq, H_est);
							//printf("sigma2 = %lf , noise2 = %lf\n", sigma *sigma, noise2);
						}
						gettimeofday(&chest_end, NULL); //______________________
						runtime.chest += (chest_end.tv_sec - chest_begin.tv_sec) + (chest_end.tv_usec - chest_begin.tv_usec) / 1000000.0;

						//2.3  信号检测
						gettimeofday(&sigdect_begin, NULL); //--------------------sigdect

						if (H_Type == 1)
						{ // 估计信道
							if (layerNum == 1)
							{
								CalSymb_mmse_1(H_est, y, RxAntNum, CarrierNum, symbolNum, sqrt(noise2), 1, 1, x_est, SymbVar, SINR_est);
							}
							else
							{
								CalSymb_mmse(H_est, y, RxAntNum, CarrierNum, layerNum, symbolNum, sqrt(noise2), 1, 1, x_est, SymbVar, SINR_est);
							}
						}
						else if (H_Type == 2)
						{ // 理想信道导频部分
							if (layerNum == 1)
							{
								CalSymb_mmse_1(H[t], y, RxAntNum, CarrierNum, symbolNum, sqrt(noise2), 1, 1, x_est, SymbVar, SINR_est);
							}
							else
							{
								CalSymb_mmse(H[t], y, RxAntNum, CarrierNum, layerNum, symbolNum, sqrt(noise2), 1, 1, x_est, SymbVar, SINR_est);
							}
						}
						else
						{ // 理想信道
							if (layerNum == 1)
							{
								CalSymb_mmse_1(H[t], y, RxAntNum, CarrierNum, symbolNum, sigma, 1, 1, x_est, SymbVar, SINR_est);
							}
							else
							{
								CalSymb_mmse_ideal(H[t], y, RxAntNum, CarrierNum, layerNum, symbolNum, 0.001, 1, 1, x_est, SymbVar, SINR_est);
							}
						}
						// for(int i=0;i<100;i++)
						// {
						// 	printf("%f+%fi  %f+%fi\n",H_est[RxAntNum * MaxBeam * CarrierNum * 3 + i].real,H_est[RxAntNum * MaxBeam * CarrierNum *3 + i].imag,H[t][i].real,H[t][i].imag);
						// }
						// 找非零项
						// for(int i=0;i<RxAntNum * LayerNum * CarrierNum * 14;i++)
						// {
						// 	if(H_est[i].real!=0)
						// 		printf("%d ",i/(RxAntNum * LayerNum * CarrierNum));
						// }

						//for(int i = 0; i< 10; i++) printf("%d : %f+%fi  %f+%fi %e\n", i, x[i].real, x[i].imag, x_est[i].real,x_est[i].imag,SymbVar[i]);

						gettimeofday(&sigdect_end, NULL); //______________________
						runtime.sigdect += (sigdect_end.tv_sec - sigdect_begin.tv_sec) + (sigdect_end.tv_usec - sigdect_begin.tv_usec) / 1000000.0;
						/*
	bzero(SymbVarM,sizeof(float) * MaxBeam);
	for(int ns = 0; ns < symbolNum; ns++){
		for(int nc = 0; nc < CarrierNum; nc++){
			for(int nl = 0; nl < layerNum; nl++){
				SymbVarM[nl] += SINR_est[ns * CarrierNum * layerNum + nc * layerNum + nl];
			}
		}
	}
	for(int i = 0; i < layerNum; i++){
		SymbVarM[i] /= symbolNum * CarrierNum;
		SINR[i] = 10 * log10(SymbVarM[i]);
		//printf("layer%d : SymbVarM = %lf SINR = %lf\n", i, SymbVarM[i], SINR[i]);
	}
*/
						gettimeofday(&cqi_begin, NULL); //--------------------cqi
						//printf("CQI_feedback : ");
						if (LinkAdptState == 1)
							RxAdptLink_lut(H_est, SINR_est, layerNum, RxAntNum, CarrierNum, 1, symbolNum, CQI_index);
						else
							RxAdptLink_lut(H_est, SINR_est, layerNum, RxAntNum, CarrierNum, 1, symbolNum, CQI_index_);
						//for (int i = 0; i < layerNum ; i++) printf("%d ", CQI_index[i]);
						//printf("\n");
						gettimeofday(&cqi_end, NULL); //______________________
						runtime.cqi += (cqi_end.tv_sec - cqi_begin.tv_sec) + (cqi_end.tv_usec - cqi_begin.tv_usec) / 1000000.0;

						//2.4  解符号映射
						for (int i = 0; i < layerNum; i++)
						{
							mod_data_len_rx[i] = max_symbol_num;
						}
						j = 0;
						k = 0;
						for (int ns = 0; ns < symbolNum; ns++)
						{
							if ((ns != 3) && (ns != 10))
							{
								for (int nc = 0; nc < CarrierNum; nc++)
								{
									for (int i = 0; i < layerNum; i++)
									{
										j = ns * CarrierNum * layerNum + nc * layerNum + i;
										if (k < mod_data_len_tx[i])
										{
											mod_data_rx[i][k] = x_est[j];
											Sigma2[i][k] = SymbVar[j];
										}
									}
									k++;
								}
							}
						}

						for (int i = 0; i < layerNum; i++)
						{
							for (int n = 0; n < mod_data_len_rx[i] * SymbolBitN[i]; n++)
							{
								demod_LLRA[i][n] = 0;
							}
						}

						gettimeofday(&demod_begin, NULL); //--------------------demod

						for (int i = 0; i < layerNum; i++)
						{
							//QAM_Demodulation(demod_LLRD[i], mod_data_rx[i], Sigma2[i], demod_LLRA[i], mod_data_len_rx[i], SymbolBitN[i], 1);
							QAM_Demod_lut(demod_LLRD[i], mod_data_rx[i], Sigma2[i], demod_LLRA[i], mod_data_len_rx[i], SymbolBitN[i], 1);
							//printf("\n%d demodulated symblos in layer %d:", mod_data_len_rx[i] * SymbolBitN[i], i);
							//for(int j = 0; j < 10; j++) printf("\nlayer %d symb :%f+%fi",i,mod_data_rx[i][j].real, mod_data_rx[i][j].imag);
						}

						gettimeofday(&demod_end, NULL); //______________________
						runtime.demod += (demod_end.tv_sec - demod_begin.tv_sec) + (demod_end.tv_usec - demod_begin.tv_usec) / 1000000.0;
						//2.5  解扰

						//2.6  解码块级联(demod_LLRD -> rm_data_rx)
						for (int i = 0; i < layerNum; i++)
						{
							rm_data_len_rx[i] = mod_data_len_rx[i] * SymbolBitN[i];
							//printf("\n%d CB concatenation bits in layer %d:", rm_data_len_rx[i], i);
							for (int n = 0; n < rm_data_len_rx[i]; n++)
							{
								rm_data_rx[i][n] = demod_LLRD[i][n];
								//printf("%d",rm_data_rx[i][n]);
							}
							cb_rx[i] = cb_tx[i];
							//printf("\nCB Segmentation in layer %d: TBS=%d, C=%d, C+=%d K+=%d, C-=%d, K-=%d, F=%d", i, cb_rx[i].tbs, cb_rx[i].C, cb_rx[i].C1, cb_rx[i].K1, cb_rx[i].C2, cb_rx[i].K2, cb_rx[i].F);
						}
						//2.7  对每个码块进行解速率匹配、Turbo译码、CRC校验、最后解码块分割(rm_data_rx -> cr_rm_data_rx -> cr_tcod_data_rx -> cr_data_rx -> data_rx)
						//srslte_tdec_init(&tdec, 6144);
						srslte_crc_init(&crc_p, crc_24B, crc_length);
						for (int i = 0; i < layerNum; ++i)
						{
							s = 0;
							j = 0;
							Lamda = max_symbol_num % cb_rx[i].C;
							for (int r = 0; r < cb_rx[i].C; ++r)
							{
								if (r < cb_rx[i].C - Lamda)
									cr_rm_data_len_rx[i] = SymbolBitN[i] * (max_symbol_num / cb_rx[i].C);
								else
									cr_rm_data_len_rx[i] = SymbolBitN[i] * ((max_symbol_num - 1) / cb_rx[i].C + 1);
								Kr = (r < cb_tx[i].C2) ? cb_tx[i].K2 : cb_tx[i].K1;
								//printf("\nlayer %d CB %d : %d decbc bits:", i, r, cr_rm_data_len_rx[i]);
								for (k = 0; k < cr_rm_data_len_rx[i]; ++k)
								{
									cr_rm_data_rx[i][k] = rm_data_rx[i][s];
									//printf("%f ",cr_rm_data_rx[i][k]);
									++s;
								}

								cr_tdec_data_len_rx[i] = 3 * Kr + 12;

								gettimeofday(&derm_begin, NULL); //--------------------derm
																 /*
			bzero(w_buff_rx[i],sizeof(float) * 6176 * 3);
			srslte_rm_turbo_rx(w_buff_rx[i], BUFFSZ, cr_rm_data_rx[i], cr_rm_data_len_rx[i], cr_tdec_data_f_rx[i], cr_tdec_data_len_rx[i], 0, 0);
			//printf("\nlayer %d CB %d : %d derm bits:", i, r, cr_tdec_data_len_rx[i]);
			for (int n = 0; n < cr_tdec_data_len_rx[i]; n++){
				cr_tdec_data_i_rx[i][n] = cr_tdec_data_f_rx[i][n];
				//printf("%f ",cr_tdec_data_f_rx[i][n]);
			}
//*/
																 //*lut
								bzero(cr_tdec_data_i_rx[i], 6176 * 3 * sizeof(int16_t));
								srslte_vec_convert_fi(cr_rm_data_rx[i], cr_rm_datai_rx[i], 64.0 / 64, cr_rm_data_len_rx[i]);
								for (int n = 0; n < cr_rm_data_len_rx[i]; n++)
								{
									//cr_rm_datai_rx[i][n] = cr_rm_data_rx[i][n];
									//printf("%f ",cr_tdec_data_f_rx[i][n]);
								}
								//srslte_vec_convert_fi(cr_rm_data_rx[i], cr_rm_datai_rx[i], 0.1, cr_rm_data_len_rx[i]);
								/*if(r == 1){
				for(int n = 1030; n < 1080; n++) printf("%d ",cr_rm_datai_rx[i][n]);
				printf("\n");
			}*/
								srslte_rm_turbo_rx_lut(cr_rm_datai_rx[i], cr_tdec_data_i_rx[i], cr_rm_data_len_rx[i], detc_cb_sizes[Kr], 0);
								//*/
								gettimeofday(&derm_end, NULL); //______________________
								runtime.derm += (derm_end.tv_sec - derm_begin.tv_sec) + (derm_end.tv_usec - derm_begin.tv_usec) / 1000000.0;

								//printf("\nLayer %d CB %d : ", i, r);
								//for(int num = 0; num < 10; num++) printf("%d ",cr_tdec_data_i_rx[i][num]);
								for (int num = 0; num < 3 * Kr + 12; num++)
								{
									if (cr_tdec_data_i_rx[i][num] > 64)
										cr_tdec_data_i_rx[i][num] = 64;
									if (cr_tdec_data_i_rx[i][num] < -64)
										cr_tdec_data_i_rx[i][num] = -64;
								}
								gettimeofday(&tdec_begin, NULL); //--------------------tdec

								//srslte_tdec_run_all(&tdec, cr_tdec_data_i_rx[i], cr_data_rx[i], 4, Kr);
								srslte_tdec_run_all(&tdec, cr_tdec_data_i_rx[i], cr_data_bytes_rx[i], 4, Kr);
								//printf("\nlayer %d CB %d : %d todec bits:", i, r, Kr);
								//for (int n = 0; n < Kr ; n++) printf("%d",cr_data_rx[i][n]);

								gettimeofday(&tdec_end, NULL); //______________________
								runtime.tdec += (tdec_end.tv_sec - tdec_begin.tv_sec) + (tdec_end.tv_usec - tdec_begin.tv_usec) / 1000000.0;

								if (cb_rx[i].C > 1)
								{

									gettimeofday(&crccheck_begin, NULL); //--------------------crccheck

									//cr_checksum[i] = srslte_crc_checksum(&crc_p, cr_data_rx[i], Kr);
									cr_checksum[i] = srslte_crc_checksum_byte(&crc_p, cr_data_bytes_rx[i], Kr);

									gettimeofday(&crccheck_end, NULL); //______________________
									runtime.crccheck += (crccheck_end.tv_sec - crccheck_begin.tv_sec) + (crccheck_end.tv_usec - crccheck_begin.tv_usec) / 1000000.0;

									if (cr_checksum[i] != 0)
									{
										//printf("crc check error in layer %d CB %d\n",i,r);
										//break;
									}
									//printf("\nlayer %d CB %d crc check : %x", i, r, cr_checksum[i]);
									for (int n = (r == 0) ? cb_tx[i].F / 8 : 0; n < (Kr - crc_length) / 8; n++)
									{
										data_bytes_rx[i][j] = cr_data_bytes_rx[i][n];
										j++;
									}
								}
								else
								{
									for (int n = cb_tx[i].F / 8; n < Kr / 8; n++)
									{
										data_bytes_rx[i][j] = cr_data_bytes_rx[i][n];
										j++;
									}
								}
								data_len_rx[i] = j * 8;
							}
							//printf("\n%d decbs bits in layer %d:", data_len_rx[i], i);
							//for (int n = 0; n < data_len_rx[i]; n++) printf("%d",data_rx[i][n]);
						}
						//2.8  CRC校验

						gettimeofday(&crccheck_begin, NULL); //--------------------crccheck

						srslte_crc_init(&crc_p, crc_24A, crc_length);
						for (int i = 0; i < layerNum; ++i)
						{
							checksum[i] = 0;
							checksum[i] = srslte_crc_checksum_byte(&crc_p, data_bytes_rx[i], data_len_rx[i]);
							//printf("\nlayer %d crc check result:%x",i,checksum[i]);
							//printf("\n%d final bits in layer %d:",data_len_rx[i] - crc_length,i);
							//for (int n = 0; n < 80; n++) printf("%d",data_rx[i][n]);
						}

						gettimeofday(&crccheck_end, NULL); //______________________
						runtime.crccheck += (crccheck_end.tv_sec - crccheck_begin.tv_sec) + (crccheck_end.tv_usec - crccheck_begin.tv_usec) / 1000000.0;

						//printf("RX is finished...\n");

						gettimeofday(&rx_end, NULL); //______________________
						runtime.rx += (rx_end.tv_sec - rx_begin.tv_sec) + (rx_end.tv_usec - rx_begin.tv_usec) / 1000000.0;

						// 解包
						for (int i = 0; i < layerNum; ++i)
							srslte_bit_unpack_vector(data_bytes_rx[i], data_rx[i], data_len_rx[i]);

						// 计算误码率
						error_num = 0;
						data_num = 0;
						for (int i = 0; i < layerNum; ++i)
						{
							error_num_l[i] = 0;
							for (j = 0; j < data_len_rx[i] - 24; j++)
							{
								data_num++;
								if (data_tx[i][j] != data_rx[i][j])
								{
									error_num_l[i]++;
									error_num++;
									error_all++;
								}
							}
							if (error_num_l[i] != 0 || checksum[i] != 0)
							{
								block_error_all++;
								layer_check[i]++;
							}
							//printf("error in layer %d : %f(%d/%d)\n", i, error_num_l[i] * 1.0 / data_len_rx[i],error_num_l[i],data_len_rx[i]);
						}
						//printf("subframe %d error : %f(%d/%d)\n", w * subframeNum + t, error_num * 1.0 / data_num, error_num, data_num);
					}
				}
				errors = error_all * 1.0 / bits_all;
				block_errors = block_error_all * 1.0 / block_all;
				fprintf(fp_sg, "%f\n", SNR);
				fprintf(fp_er, "%f\n", errors);
				fprintf(fp_ber, "%f\n", block_errors);
				index++;
				printf("\n====================\n\n1.Error rate statistics\n\n");
				if (LinkAdptState == 1)
					printf("test %d SNR = %.2f sigma = %e\nbits error:%f(%d/%d)\nblocks error:%f(%d/%d)\n",
						   index, SNR, sigma, error_all * 1.0 / bits_all, error_all, bits_all, block_errors, block_error_all, block_all);
				else
					printf("test %d CQI = %d SNR = %.2f sigma = %e\nbits error:%f(%d/%d)\nblocks error:%f(%d/%d)\n",
						   index, c, SNR, sigma, error_all * 1.0 / bits_all, error_all, bits_all, block_errors, block_error_all, block_all);
				//for (int i = 0; i < layerNum; ++i) printf("%d ",layer_check[i]);
				//printf("\n");
				//*
				if (SNR_test == SNR_max && step_test == step - 1)
				{
					runtime.others_tx = runtime.tx - runtime.crc - runtime.cbsegm - runtime.tcod - runtime.rm - runtime.mod - runtime.pack;
					runtime.others_rx = runtime.rx - runtime.chest - runtime.sigdect - runtime.demod - runtime.cqi - runtime.derm - runtime.tdec - runtime.crccheck;
					printf("\n2.Running time statistics(%d subframes):\n", loopNum);
					printf("\n");
					printf("CRC attachment     :%8.4fs (%5.2f%%)\n", runtime.crc, runtime.crc / runtime.tx * 100);
					printf("Cb segmentation    :%8.4fs (%5.2f%%)\n", runtime.cbsegm, runtime.cbsegm / runtime.tx * 100);
					printf("Turbo encoder      :%8.4fs (%5.2f%%)\n", runtime.tcod, runtime.tcod / runtime.tx * 100);
					printf("TX Rate matching   :%8.4fs (%5.2f%%)\n", runtime.rm, runtime.rm / runtime.tx * 100);
					printf("Modulation         :%8.4fs (%5.2f%%)\n", runtime.mod, runtime.mod / runtime.tx * 100);
					printf("Packing            :%8.4fs (%5.2f%%)\n", runtime.pack, runtime.pack / runtime.tx * 100);
					printf("Others in TX       :%8.4fs (%5.2f%%)\n", runtime.others_tx, runtime.others_tx / runtime.tx * 100);
					printf("                                    \n");
					printf("Channel estimation :%8.4fs (%5.2f%%)\n", runtime.chest, runtime.chest / runtime.rx * 100);
					printf("Signal Detection   :%8.4fs (%5.2f%%)\n", runtime.sigdect, runtime.sigdect / runtime.rx * 100);
					printf("Link adaptation    :%8.4fs (%5.2f%%)\n", runtime.cqi, runtime.cqi / runtime.rx * 100);
					printf("Demodulation       :%8.4fs (%5.2f%%)\n", runtime.demod, runtime.demod / runtime.rx * 100);
					printf("RX Rate matching   :%8.4fs (%5.2f%%)\n", runtime.derm, runtime.derm / runtime.rx * 100);
					printf("Turbo decoder      :%8.4fs (%5.2f%%)\n", runtime.tdec, runtime.tdec / runtime.rx * 100);
					printf("CRC check          :%8.4fs (%5.2f%%)\n", runtime.crccheck, runtime.crccheck / runtime.rx * 100);
					printf("Others in RX       :%8.4fs (%5.2f%%)\n", runtime.others_rx, runtime.others_rx / runtime.rx * 100);
					printf("\n");
					printf("Total time in TX   :%8.4fs          \n", runtime.tx, runtime.tx / runtime.tx * 100);
					printf("Total time in RX   :%8.4fs          \n", runtime.rx, runtime.rx / runtime.rx * 100);
					printf("\n");
					printf("Throughput in TX   :%8.4fMbps       \n", bits_all / runtime.tx / 1000000);
					printf("Throughput in RX   :%8.4fMbps       \n", bits_all / runtime.rx / 1000000);
					printf("\n");
				}
				//*/
			}
		}
	}
	freeQAMtable();
	freeAdptLinktable();
	srslte_tcod_free(&tcod);
	srslte_tdec_free(&tdec);
	fclose(fp_sg);
	fclose(fp_er);
	fclose(fp_ber);
	fclose(fp_nr);
	fclose(fp_ni);
	free(PilotSymb);
	free(x);
	free(y);
	free(x_temp);
	free(H_temp);
	free(y_temp);
	free(H_est);
	free(x_est);
	free(SymbVar);

	free(parity_bytes);
	for (int i = 0; i < layerNum; i++)
	{
		free(data_tx[i]);
		free(data_bytes_tx[i]);
		free(cr_data_tx[i]);
		free(cr_data_bytes_tx[i]);
		free(cr_tcod_data_tx[i]);
		free(cr_rm_data_tx[i]);
		free(cr_rm_data_bytes_tx[i]);
		free(w_buff_tx[i]);
		free(rm_data_tx[i]);
		free(mod_data_tx[i]);

		free(mod_data_rx[i]);
		free(Sigma2[i]);
		free(demod_LLRD[i]);
		free(demod_LLRA[i]);
		free(rm_data_rx[i]);
		free(cr_rm_data_rx[i]);
		free(cr_rm_datai_rx[i]);
		free(w_buff_rx[i]);
		free(cr_tdec_data_i_rx[i]);
		free(cr_tdec_data_f_rx[i]);
		free(cr_data_rx[i]);
		free(cr_data_bytes_rx[i]);
		free(data_rx[i]);
		free(data_bytes_rx[i]);
	}

	free(gwn);
	for (int t = 0; t < subframeNum; t++)
	{
		free(R_DCT[t]);
		free(H[t]);
	}
	return 0;
}
