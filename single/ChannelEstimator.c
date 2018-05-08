//输入：
//接收信号的实部和虚部，导频信号的实部和虚部，信道相关阵的实部和虚部；
//发射天线的数目、接收天线的数目、载波数目、最大波束数目、导频符号数目、数据符号数目、路径数目、噪声方差、
//输出：
//信道矩阵、干扰加噪声的协方差矩阵

//信道相关矩阵   R_DCT (RxAntNum*UserNum,MaxBeam*UserNum,CarrierNum);
//接收信号矩阵    signalRec(RxAntNum，CarrierNum,)
//导频符号   PilotSymb    (TxAntNum,PilotSymbNum)

//返回的信道估计值 H_est (RxAntNum,TxAntNum,CarrierNum)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mkl.h"

#define M_PI 3.14159265358979323846
#define CLOCKS_PER_SEC ((clock_t)1000)

void Cal_DCTMx(lapack_complex_float *DCT_Mx, int MxNum);
void InterPolation_Time(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *ColIndex, int SymbNum, int PilotSymbNum);
void InterPolation_Freq(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *RowIndex, int SymbNum, int PilotFreqNum);
void Cal_DCT(lapack_complex_float *DCTMatrix, lapack_complex_float *Matrix, float *TrigoMx, int M, int N);
void Cal_IDCT(lapack_complex_float *IDCTMatrix, lapack_complex_float *Matrix, float *TrigoMx, int M, int N);

void Cal_DCT2(lapack_complex_float *DCTMatrix, lapack_complex_float *Matrix, int M);
void Cal_IDCT2(lapack_complex_float *IDCTMatrix, lapack_complex_float *Matrix, int M);

void Cal_TrigoMx(float *TrigoMx, int PilotFreqNum);

void ChannelEstimator_DCT(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, float *R_DCT, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int MaxBeam, int PilotSymbNum, int SymbNum, int inter_freq, float sigma, lapack_complex_float *CFR_est)
{
	//inter_freq 代表在频域放置导频的位置
	int i, j, p, k, t, s;
	float a;
	int PilotFreqNum;
	int *PilotFreqIndex;
	//lapack_complex_float alpha;
	//lapack_complex_float beta;

	//lapack_complex_float *DCT_Mx;
	//float *TrigoMx;
	float *PG_i;
	float *PG_est;
	lapack_complex_float *SignalRecPilot;
	lapack_complex_float *Pilot_nTx;
	lapack_complex_float *CFR_est_Pilot;
	lapack_complex_float *h_dct;
	lapack_complex_float *CFR_est_nCh;

	PilotFreqNum = CarrierNum / inter_freq;
	PilotFreqIndex = (int *)malloc(sizeof(int) * PilotFreqNum);
	//DCT_Mx = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum*PilotFreqNum);
	//TrigoMx = (float*)malloc(sizeof(float)*PilotFreqNum*PilotFreqNum);
	PG_i = (float *)malloc(sizeof(float) * PilotFreqNum);
	PG_est = (float *)malloc(sizeof(float) * PilotFreqNum);
	SignalRecPilot = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * PilotFreqNum);
	Pilot_nTx = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * PilotFreqNum);	 //
	CFR_est_Pilot = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * PilotFreqNum); //
	h_dct = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * PilotFreqNum);		 //
	CFR_est_nCh = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * CarrierNum * SymbNum);

	//Cal_DCTMx(DCT_Mx, PilotFreqNum);
	//Cal_TrigoMx(TrigoMx, PilotFreqNum);
	//print_arr(PilotFreqNum, 5, "DCT_Mx", DCT_Mx);

	for (i = 0; i < RxAntNum; ++i)
	{
		for (j = 0; j < TxAntNum; ++j)
		{
			//信道矩阵的初始化
			for (k = 0; k < CarrierNum; ++k)
			{
				for (s = 0; s < SymbNum; ++s)
				{
					CFR_est_nCh[s * CarrierNum + k].real = 0;
					CFR_est_nCh[s * CarrierNum + k].imag = 0;
				}
			}
			//计算时域位置3和10的信道估计值
			for (p = 0; p < PilotSymbNum; ++p)
			{
				//计算导频在频域的位置
				if (inter_freq == 1)
				{
					for (k = 0; k < PilotFreqNum; ++k)
					{
						PilotFreqIndex[k] = k;
					}
				}
				else
				{
					for (k = 0; k < PilotFreqNum; ++k)
					{
						PilotFreqIndex[k] = j + k * inter_freq;
					}
				}
				//获取导频信号
				//PilotSymb(TxAntNum, CarrierNum，PilotSymbNum) 函数的输入
				for (k = 0; k < PilotFreqNum; ++k)
				{
					Pilot_nTx[k] = PilotSymb[p * CarrierNum * TxAntNum + PilotFreqIndex[k] * TxAntNum + j];
				}
				//print_clx(PilotFreqNum, 1, "Pilot_nTx", Pilot_nTx);
				//获取接收信号
				for (k = 0; k < PilotFreqNum; ++k)
				{
					//signalRec_R(RxAntNum, CarrierNum, SymbNum)
					SignalRecPilot[k] = SignalRec[PilotSymbIndex[p] * CarrierNum * RxAntNum + PilotFreqIndex[k] * RxAntNum + i];
				}
				//print_clx(PilotFreqNum, 1, "SignalRec_Pilot", SignalRecPilot);

				for (k = 0; k < PilotFreqNum; ++k)
				{
					// PilotSymb    (TxAntNum,PilotSymbNum)
					a = Pilot_nTx[k].real * Pilot_nTx[k].real + Pilot_nTx[k].imag * Pilot_nTx[k].imag;
					CFR_est_Pilot[k].real = (SignalRecPilot[k].real * Pilot_nTx[k].real + SignalRecPilot[k].imag * Pilot_nTx[k].imag) / a;
					CFR_est_Pilot[k].imag = (SignalRecPilot[k].imag * Pilot_nTx[k].real - SignalRecPilot[k].real * Pilot_nTx[k].imag) / a;
				}
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//判断是否存在干扰用户
				//初始化
				for (k = 0; k < PilotFreqNum; ++k)
				{
					PG_i[k] = sigma * sigma;
				}
				//print_arr(PilotFreqNum, 1, "PG_i", PG_i);
				if (UserNum > 1)
				{
					for (t = 0; t < PilotFreqNum; ++t)
					{
						//R   (RxAntNum*UserNum,MaxBeam*UserNum,CarrierNum/inter_freq,SymbolNum);
						for (s = 1; s < UserNum; ++s)
						{
							PG_i[t] += R_DCT[PilotSymbIndex[p] * PilotFreqNum * RxAntNum * UserNum * MaxBeam * UserNum + t * RxAntNum * UserNum * MaxBeam * UserNum + (s * MaxBeam + j) * RxAntNum * UserNum + i];
						}
					}
				}
				//print_arr(PilotFreqNum, 1, "PG_i", PG_i);

				for (k = 0; k < PilotFreqNum; ++k)
				{
					//PG[k] = R_DCT[i*MaxBeam*UserNum*PilotCarrierNum + j*PilotCarrierNum + k];
					a = R_DCT[PilotSymbIndex[p] * PilotFreqNum * RxAntNum * UserNum * MaxBeam * UserNum + k * RxAntNum * UserNum * MaxBeam * UserNum + j * RxAntNum * UserNum + i];
					PG_est[k] = a / (a + PG_i[k]);
				}
				//print_arr(PilotFreqNum, 1, "PG_est", PG_est);

				////1. 信道矩阵的DCT变换
				//for (k = 0; k < PilotFreqNum; ++k) {
				//	h_dct[k].real = 0;
				//	h_dct[k].imag = 0;
				//}
				//for (k = 0; k < PilotFreqNum; ++k) {
				//	for (t = 0; t < PilotFreqNum; ++t) {
				//		h_dct[k].real += DCT_Mx[t*PilotFreqNum + k].real * CFR_est_Pilot[t].real;
				//		h_dct[k].imag += DCT_Mx[t*PilotFreqNum + k].real * CFR_est_Pilot[t].imag;
				//	}
				//}
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				////2. 使用MKL库函数
				//alpha.real = 1;
				//alpha.imag = 0;
				//beta.real = 0;
				//beta.imag = 0;
				//cblas_zgemv(CblasColMajor, CblasNoTrans, PilotFreqNum, PilotFreqNum, &alpha, DCT_Mx, PilotFreqNum, CFR_est_Pilot, 1, &beta, h_dct, 1);
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				////3. 使用DCT变换的方法
				//for (k = 0; k < PilotFreqNum; ++k) {
				//	h_dct[k].real = 0;
				//	h_dct[k].imag = 0;
				//}
				//Cal_DCT(h_dct, CFR_est_Pilot, TrigoMx, PilotFreqNum, 1);
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				//4. 使用FFT变换的方法实现DCT
				for (k = 0; k < PilotFreqNum; ++k)
				{
					h_dct[k].real = 0;
					h_dct[k].imag = 0;
				}
				Cal_DCT2(h_dct, CFR_est_Pilot, PilotFreqNum);
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				for (k = 0; k < PilotFreqNum; ++k)
				{
					h_dct[k].real = PG_est[k] * h_dct[k].real;
					h_dct[k].imag = PG_est[k] * h_dct[k].imag;
					//初始化估计的信道矩阵
					CFR_est_Pilot[k].real = 0;
					CFR_est_Pilot[k].imag = 0;
				}
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				////1.IDCT
				//for (k = 0; k < PilotFreqNum; ++k) {
				//	for (t = 0; t < PilotFreqNum; ++t) {
				//		CFR_est_Pilot[k].real += DCT_Mx[k*PilotFreqNum + t].real * h_dct[t].real;
				//		CFR_est_Pilot[k].imag += DCT_Mx[k*PilotFreqNum + t].real * h_dct[t].imag;
				//	}
				//}
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//2使用MKL库函数的方法
				//cblas_zgemv(CblasColMajor, CblasConjTrans, PilotFreqNum, PilotFreqNum, &alpha, DCT_Mx, PilotFreqNum, h_dct, 1, &beta, CFR_est_Pilot, 1);
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//for (k = 0; k < PilotFreqNum; ++k) {
				//	CFR_est_Pilot[k].real = 0;
				//	CFR_est_Pilot[k].imag = 0;
				//}

				//3使用IDCT变换的方法
				//Cal_IDCT(CFR_est_Pilot, h_dct, TrigoMx, PilotFreqNum, 1);
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//4. 使用IFFT的方法 实现DCT
				Cal_IDCT2(CFR_est_Pilot, h_dct, PilotFreqNum);
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//线性插值,复数的线性插值方法
				for (k = 0; k < PilotFreqNum; ++k)
				{
					CFR_est_nCh[PilotSymbIndex[p] * CarrierNum + PilotFreqIndex[k]].real = CFR_est_Pilot[k].real;
					CFR_est_nCh[PilotSymbIndex[p] * CarrierNum + PilotFreqIndex[k]].imag = CFR_est_Pilot[k].imag;
				}
				//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);
				//InterPolation_Freq(CFR_est_nCh, CarrierNum, PilotFreqIndex, SymbNum, PilotFreqNum);
				//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);
				//print_clx(CarrierNum, 4, "CFR_est_nCh", CFR_est_nCh);
			}
			//频域插值
			if (inter_freq != 1)
			{
				InterPolation_Freq(CFR_est_nCh, CarrierNum, PilotFreqIndex, SymbNum, PilotFreqNum);
			}
			//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);
			//时域插值
			//if (PilotSymbNum != SymbNum) {
			//	InterPolation_Time(CFR_est_nCh, CarrierNum, PilotSymbIndex, SymbNum, PilotSymbNum);
			//}

			////使用MKL库函数进行复制
			//for (p = 0; p < 7; ++p) {
			//	cblas_zcopy(CarrierNum, &CFR_est_nCh[PilotSymbIndex[0] * CarrierNum], 1, &CFR_est_nCh[p * CarrierNum], 1);
			//for(p = 7; p < SymbNum; ++p){
			//	cblas_zcopy(CarrierNum, &CFR_est_nCh[PilotSymbIndex[1] * CarrierNum], 1, &CFR_est_nCh[p * CarrierNum], 1);
			//}

			for (p = 0; p < 7; ++p)
			{
				for (k = 0; k < CarrierNum; ++k)
				{
					CFR_est_nCh[p * CarrierNum + k] = CFR_est_nCh[PilotSymbIndex[0] * CarrierNum + k];
				}
			}
			for (p = 7; p < SymbNum; ++p)
			{
				for (k = 0; k < CarrierNum; ++k)
				{
					CFR_est_nCh[p * CarrierNum + k] = CFR_est_nCh[PilotSymbIndex[1] * CarrierNum + k];
				}
			}

			//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);

			//H_est (RxAntNum,TxAntNum,CarrierNum,SymbNum)
			for (t = 0; t < CarrierNum * SymbNum; ++t)
			{
				CFR_est[t * RxAntNum * TxAntNum + j * RxAntNum + i] = CFR_est_nCh[t];
			}
		}
	}
	//free(DCT_Mx);
	//free(TrigoMx);
	free(PilotFreqIndex);
	free(PG_i);
	free(PG_est);
	free(SignalRecPilot);
	free(Pilot_nTx);
	free(CFR_est_Pilot);
	free(CFR_est_nCh);
	free(h_dct);
}

void Cal_DCTMx(lapack_complex_float *DCT_Mx, int MxNum)
{
	int i, j;
	float N;
	float cu1;
	float cu2;

	N = MxNum;
	cu1 = sqrt(1 / N);
	cu2 = sqrt(2 / N);
	for (i = 0; i < MxNum; ++i)
	{
		for (j = 0; j < MxNum; ++j)
		{
			if (i == 0)
			{
				DCT_Mx[j * MxNum + i].real = cu1 * cos(M_PI * (j + 0.5) * i / N);
				DCT_Mx[j * MxNum + i].imag = 0;
			}
			else
			{
				DCT_Mx[j * MxNum + i].real = cu2 * cos(M_PI * (j + 0.5) * i / N);
				DCT_Mx[j * MxNum + i].imag = 0;
			}
		}
	}
}

void Cal_TrigoMx(float *TrigoMx, int PilotFreqNum)
{
	int i, j;
	for (i = 0; i < PilotFreqNum; ++i)
	{
		for (j = 0; j < PilotFreqNum; ++j)
		{
			TrigoMx[j * PilotFreqNum + i] = cos(M_PI * (j + 0.5) * i / PilotFreqNum);
		}
	}
}

void Cal_DCT(lapack_complex_float *DCTMatrix, lapack_complex_float *Matrix, float *TrigoMx, int M, int N)
{
	int u, v, i;
	float cu1 = sqrt(1 / (float)M);
	float cu2 = sqrt(2 / (float)M);
	//u == 0
	for (v = 0; v < N; ++v)
	{
		for (i = 0; i < M; ++i)
		{
			DCTMatrix[v * M].real += cu1 * Matrix[v * M + i].real * TrigoMx[i * M];
			DCTMatrix[v * M].imag += cu1 * Matrix[v * M + i].imag * TrigoMx[i * M];
		}
	}
	for (u = 1; u < M; ++u)
	{
		for (v = 0; v < N; ++v)
		{
			for (i = 0; i < M; ++i)
			{
				DCTMatrix[v * M + u].real += cu2 * Matrix[v * M + i].real * TrigoMx[i * M + u];
				DCTMatrix[v * M + u].imag += cu2 * Matrix[v * M + i].imag * TrigoMx[i * M + u];
			}
		}
	}
	return;
}

void Cal_DCT2(lapack_complex_float *DCTMatrix, lapack_complex_float *Matrix, int M)
{
	int j;
	lapack_complex_float *fft_1, *fft_2;
	DFTI_DESCRIPTOR_HANDLE my_desc1_handle;

	fft_1 = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * M);
	fft_2 = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * M);
	if (M == 1)
	{
		DCTMatrix[0] = Matrix[0];
	}
	else
	{
		for (j = 0; j < M / 2; ++j)
		{
			fft_1[j].real = Matrix[2 * j].real;
			fft_1[j].imag = 0;
			fft_1[M / 2 + j].real = Matrix[M - 2 * j - 1].real;
			fft_1[M / 2 + j].imag = 0;

			fft_2[j].real = Matrix[2 * j].imag;
			fft_2[j].imag = 0;
			fft_2[M / 2 + j].real = Matrix[M - 2 * j - 1].imag;
			fft_2[M / 2 + j].imag = 0;
		}
		//FFT变换，MKL
		DftiCreateDescriptor(&my_desc1_handle, DFTI_SINGLE, DFTI_COMPLEX, 1, M);
		DftiCommitDescriptor(my_desc1_handle);
		DftiComputeForward(my_desc1_handle, fft_1);
		DftiComputeForward(my_desc1_handle, fft_2);
		DftiFreeDescriptor(&my_desc1_handle);

		DCTMatrix[0].real = sqrt(1 / (float)M) * fft_1[0].real;
		DCTMatrix[0].imag = sqrt(1 / (float)M) * fft_2[0].real;
		for (j = 1; j < M; ++j)
		{
			DCTMatrix[j].real = sqrt(2 / (float)M) * (cos(M_PI * j / (2 * M)) * fft_1[j].real + sin(M_PI * j / (2 * M)) * fft_1[j].imag);
			DCTMatrix[j].imag = sqrt(2 / (float)M) * (cos(M_PI * j / (2 * M)) * fft_2[j].real + sin(M_PI * j / (2 * M)) * fft_2[j].imag);
		}
	}
	free(fft_1);
	free(fft_2);
}

void Cal_IDCT2(lapack_complex_float *IDCTMatrix, lapack_complex_float *Matrix, int M)
{
	int j;
	float a, b;
	float c, d;
	lapack_complex_float *fft_1, *fft_2;
	lapack_complex_float *qe;
	MKL_LONG status;
	DFTI_DESCRIPTOR_HANDLE my_desc1_handle;

	//实部和虚部分别进行IDCT变换
	fft_1 = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * M);
	fft_2 = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * M);
	qe = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * M);

	c = sqrt(M);
	d = sqrt(2 * M);
	if (M == 1)
	{
		IDCTMatrix[0] = Matrix[0];
	}
	else
	{
		//需要进行IFFT的向量
		fft_1[0].real = Matrix[0].real / c;
		fft_1[0].imag = 0;
		fft_2[0].real = Matrix[0].imag / c;
		fft_2[0].imag = 0;
		for (j = 1; j < M; ++j)
		{
			a = cos(M_PI * j / (2 * M));
			b = sin(M_PI * j / (2 * M));
			fft_1[j].real = (Matrix[j].real * a + Matrix[M - j].real * b) / d;
			fft_1[j].imag = (Matrix[j].real * b - Matrix[M - j].real * a) / d;
			fft_2[j].real = (Matrix[j].imag * a + Matrix[M - j].imag * b) / d;
			fft_2[j].imag = (Matrix[j].imag * b - Matrix[M - j].imag * a) / d;
		}
		//IFFT变换，MKL
		status = DftiCreateDescriptor(&my_desc1_handle, DFTI_SINGLE, DFTI_COMPLEX, 1, M);
		status = DftiCommitDescriptor(my_desc1_handle);
		status = DftiComputeBackward(my_desc1_handle, fft_1);
		status = DftiComputeBackward(my_desc1_handle, fft_2);
		status = DftiFreeDescriptor(&my_desc1_handle);

		for (j = 0; j < M / 2; ++j)
		{
			IDCTMatrix[2 * j].real = fft_1[j].real;
			IDCTMatrix[2 * j + 1].real = fft_1[M - 1 - j].real;
			IDCTMatrix[2 * j].imag = fft_2[j].real;
			IDCTMatrix[2 * j + 1].imag = fft_2[M - 1 - j].real;
		}
	}
	free(fft_1);
	free(fft_2);
	free(qe);
}

void Cal_IDCT(lapack_complex_float *IDCTMatrix, lapack_complex_float *Matrix, float *TrigoMx, int M, int N)
{
	int u, v, i;
	float cu1 = sqrt(1 / (float)M);
	float cu2 = sqrt(2 / (float)M);
	for (v = 0; v < N; ++v)
	{
		for (u = 0; u < M; ++u)
		{
			for (i = 0; i < M; ++i)
			{
				if (i == 0)
				{
					IDCTMatrix[v * M + u].real += cu1 * Matrix[v * M + i].real * TrigoMx[u * M + i];
					IDCTMatrix[v * M + u].imag += cu1 * Matrix[v * M + i].imag * TrigoMx[u * M + i];
				}
				else
				{
					IDCTMatrix[v * M + u].real += cu2 * Matrix[v * M + i].real * TrigoMx[u * M + i];
					IDCTMatrix[v * M + u].imag += cu2 * Matrix[v * M + i].imag * TrigoMx[u * M + i];
				}
			}
		}
	}
	return;
}

void InterPolation_Time(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *ColIndex, int SymbNum, int PilotSymbNum)
{
	int c, t, k;
	int a;
	float b_real, b_imag;
	for (k = 0; k < CarrierNum; ++k)
	{
		a = ColIndex[1] - ColIndex[0];
		b_real = (CFR_est_nCh[ColIndex[1] * CarrierNum + k].real - CFR_est_nCh[ColIndex[0] * CarrierNum + k].real) / a;
		b_imag = (CFR_est_nCh[ColIndex[1] * CarrierNum + k].imag - CFR_est_nCh[ColIndex[0] * CarrierNum + k].imag) / a;
		for (c = 0; c < ColIndex[0]; ++c)
		{
			CFR_est_nCh[c * CarrierNum + k].real = CFR_est_nCh[ColIndex[0] * CarrierNum + k].real - (ColIndex[0] - c) * b_real;
			CFR_est_nCh[c * CarrierNum + k].imag = CFR_est_nCh[ColIndex[0] * CarrierNum + k].imag - (ColIndex[0] - c) * b_imag;
		}

		for (t = 0; t < PilotSymbNum - 1; ++t)
		{
			a = ColIndex[t + 1] - ColIndex[t];
			b_real = (CFR_est_nCh[ColIndex[t + 1] * CarrierNum + k].real - CFR_est_nCh[ColIndex[t] * CarrierNum + k].real) / a;
			b_imag = (CFR_est_nCh[ColIndex[t + 1] * CarrierNum + k].imag - CFR_est_nCh[ColIndex[t] * CarrierNum + k].imag) / a;
			for (c = ColIndex[t] + 1; c < ColIndex[t + 1]; ++c)
			{
				CFR_est_nCh[c * CarrierNum + k].real = CFR_est_nCh[(c - 1) * CarrierNum + k].real + b_real;
				CFR_est_nCh[c * CarrierNum + k].imag = CFR_est_nCh[(c - 1) * CarrierNum + k].imag + b_imag;
			}
		}
		//a = ColIndex[PilotSymbNum - 1] - ColIndex[PilotSymbNum - 2];
		//b_real = (CFR_est_nCh[ColIndex[PilotSymbNum - 1] * CarrierNum + k].real - CFR_est_nCh[ColIndex[PilotSymbNum - 2] * CarrierNum + k].real) / a;
		//b_imag = (CFR_est_nCh[ColIndex[PilotSymbNum - 1] * CarrierNum + k].imag - CFR_est_nCh[ColIndex[PilotSymbNum - 2] * CarrierNum + k].imag) / a;
		for (c = ColIndex[PilotSymbNum - 1] + 1; c < SymbNum; ++c)
		{
			CFR_est_nCh[c * CarrierNum + k].real = CFR_est_nCh[(c - 1) * CarrierNum + k].real + b_real;
			CFR_est_nCh[c * CarrierNum + k].imag = CFR_est_nCh[(c - 1) * CarrierNum + k].imag + b_imag;
		}
	}
	return;
}

void InterPolation_Freq(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *RowIndex, int SymbNum, int PilotFreqNum)
{
	int c, t, k;
	int a;
	float b_real, b_imag;
	for (k = 0; k < SymbNum; ++k)
	{
		//a = RowIndex[1] - RowIndex[0];
		//b_real = (CFR_est_nCh[k*CarrierNum + RowIndex[1]].real - CFR_est_nCh[k*CarrierNum + RowIndex[0]].real) / a;
		//b_imag = (CFR_est_nCh[k*CarrierNum + RowIndex[1]].imag - CFR_est_nCh[k*CarrierNum + RowIndex[0]].imag) / a;
		for (c = 0; c < RowIndex[0]; ++c)
		{
			//CFR_est_nCh[k*CarrierNum + c].real = CFR_est_nCh[k*CarrierNum + RowIndex[0]].real - (RowIndex[0] - c) * b_real;
			//CFR_est_nCh[k*CarrierNum + c].imag = CFR_est_nCh[k*CarrierNum + RowIndex[0]].imag - (RowIndex[0] - c) * b_imag;
			CFR_est_nCh[k * CarrierNum + c].real = CFR_est_nCh[k * CarrierNum + RowIndex[0]].real;
			CFR_est_nCh[k * CarrierNum + c].imag = CFR_est_nCh[k * CarrierNum + RowIndex[0]].imag;
		}
		for (t = 0; t < PilotFreqNum - 1; ++t)
		{
			a = RowIndex[t + 1] - RowIndex[t];
			b_real = (CFR_est_nCh[k * CarrierNum + RowIndex[t + 1]].real - CFR_est_nCh[k * CarrierNum + RowIndex[t]].real) / a;
			b_imag = (CFR_est_nCh[k * CarrierNum + RowIndex[t + 1]].imag - CFR_est_nCh[k * CarrierNum + RowIndex[t]].imag) / a;
			for (c = RowIndex[t] + 1; c < RowIndex[t + 1]; ++c)
			{
				CFR_est_nCh[k * CarrierNum + c].real = CFR_est_nCh[k * CarrierNum + c - 1].real + b_real;
				CFR_est_nCh[k * CarrierNum + c].imag = CFR_est_nCh[k * CarrierNum + c - 1].imag + b_imag;
			}
		}
		//a = RowIndex[PilotFreqNum - 1] - RowIndex[PilotFreqNum - 2];
		//b_real = (CFR_est_nCh[k*CarrierNum + RowIndex[PilotFreqNum - 1]].real - CFR_est_nCh[k*CarrierNum + RowIndex[PilotFreqNum - 2]].real) / a;
		//b_real = (CFR_est_nCh[k*CarrierNum + RowIndex[PilotFreqNum - 1]].imag - CFR_est_nCh[k*CarrierNum + RowIndex[PilotFreqNum - 2]].imag) / a;
		for (c = RowIndex[PilotFreqNum - 1] + 1; c < CarrierNum; ++c)
		{
			//CFR_est_nCh[k*CarrierNum + c].real = CFR_est_nCh[k*CarrierNum + c - 1].real + b_real;
			//CFR_est_nCh[k*CarrierNum + c].imag = CFR_est_nCh[k*CarrierNum + c - 1].imag + b_imag;
			CFR_est_nCh[k * CarrierNum + c].real = CFR_est_nCh[k * CarrierNum + RowIndex[PilotFreqNum - 1]].real;
			CFR_est_nCh[k * CarrierNum + c].imag = CFR_est_nCh[k * CarrierNum + RowIndex[PilotFreqNum - 1]].imag;
		}
	}
	return;
}
