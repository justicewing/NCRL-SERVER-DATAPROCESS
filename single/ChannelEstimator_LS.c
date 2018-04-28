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

const float pi = 3.141592653;

void Cal_DCT_FFT(lapack_complex_float* DCTMatrix, lapack_complex_float* Matrix, int M);
void Cal_TrigoMx(float* TrigoMx, int PilotFreqNum);
void Cal_DCT(lapack_complex_float* DCTMatrix, lapack_complex_float* Matrix,float* TrigoMx, int M, int N);
void InterPolation_Time(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *ColIndex, int SymbNum, int PilotSymbNum);
void InterPolation_Freq(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *RowIndex, int SymbNum, int PilotFreqNum);
void Inter_Freq2(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *PilotFreqIndex, int *PilotSymbIndex, int PilotFreqNum, int PilotSymbNum);

float ChannelEstimator_LS(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int PilotSymbNum, int SymbNum, int inter_freq, lapack_complex_float *CFR_est)
{
	//inter_freq 代表在频域放置导频的位置
	int i, j, p, k, t, s;
	float a;
	int PilotFreqNum;
	int *PilotFreqIndex;
	float NoiseVarEst = 0;
	int NoiseRow = 1;
	
	lapack_complex_float *SignalRecPilot;
	lapack_complex_float *Pilot_nTx;
	lapack_complex_float *CFR_est_Pilot;
	lapack_complex_float *h_dct;
	lapack_complex_float *CFR_est_nCh;
	float *TrigoMx;

	PilotFreqNum = CarrierNum / inter_freq;
	PilotFreqIndex = (int*)malloc(sizeof(int)*PilotFreqNum);
	SignalRecPilot = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);
	Pilot_nTx = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	CFR_est_Pilot = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	h_dct = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	CFR_est_nCh = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*CarrierNum*SymbNum);
	TrigoMx = (float*)malloc(sizeof(float)*PilotFreqNum*PilotFreqNum);
	
	int fft_type = 0;
	if(PilotFreqNum % 2 == 0) fft_type = 1;
	//Cal_DCTMx(DCT_Mx, PilotFreqNum);
	if(fft_type == 0) Cal_TrigoMx(TrigoMx, PilotFreqNum);
	//print_arr(PilotFreqNum, 5, "DCT_Mx", DCT_Mx);

	for (i = 0; i < RxAntNum; ++i) {
		for (j = 0; j < TxAntNum; ++j) {
			//信道矩阵的初始化
			/*
			for (k = 0; k < CarrierNum; ++k) {
				for (s = 0; s < SymbNum; ++s) {
					CFR_est_nCh[s*CarrierNum + k].real = 0;
					CFR_est_nCh[s*CarrierNum + k].imag = 0;
				}
			}
			*/
			//计算时域位置3和10的信道估计值
			for (p = 0; p < PilotSymbNum; ++p) {
				//计算导频在频域的位置
				if (inter_freq == 1) {
					for (k = 0; k < PilotFreqNum; ++k) {
						PilotFreqIndex[k] = k;
					}
				}
				else {
					for (k = 0; k < PilotFreqNum; ++k) {
						PilotFreqIndex[k] = j + k * inter_freq;
					}
				}
				//获取导频信号
				//PilotSymb(TxAntNum, CarrierNum，PilotSymbNum) 函数的输入
				for (k = 0; k < PilotFreqNum; ++k) {
					Pilot_nTx[k] = PilotSymb[p * CarrierNum * TxAntNum + PilotFreqIndex[k] * TxAntNum + j];
				}
				//print_clx(PilotFreqNum, 1, "Pilot_nTx", Pilot_nTx);
				//获取接收信号
				for (k = 0; k < PilotFreqNum; ++k) {
					//signalRec_R(RxAntNum, CarrierNum, SymbNum)
					SignalRecPilot[k] = SignalRec[PilotSymbIndex[p] * CarrierNum * RxAntNum + PilotFreqIndex[k] * RxAntNum + i];
				}
				//print_clx(PilotFreqNum, 1, "SignalRec_Pilot", SignalRecPilot);

				for (k = 0; k < PilotFreqNum; ++k) {
					// PilotSymb    (TxAntNum,PilotSymbNum)
					a = Pilot_nTx[k].real * Pilot_nTx[k].real + Pilot_nTx[k].imag * Pilot_nTx[k].imag;
					CFR_est_Pilot[k].real = (SignalRecPilot[k].real * Pilot_nTx[k].real + SignalRecPilot[k].imag * Pilot_nTx[k].imag) / a;
					CFR_est_Pilot[k].imag = (SignalRecPilot[k].imag * Pilot_nTx[k].real - SignalRecPilot[k].real * Pilot_nTx[k].imag) / a;
				}
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//信道矩阵的DCT变换
				if(fft_type == 0){
					for (k = 0; k < PilotFreqNum; ++k) {
						h_dct[k].real = 0;
						h_dct[k].imag = 0;
					}
					Cal_DCT(h_dct, CFR_est_Pilot, TrigoMx, PilotFreqNum, 1);
				}
				else{
					Cal_DCT_FFT(h_dct, CFR_est_Pilot, PilotFreqNum);
				}
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				//计算噪声方差
				//RxAntNum*TxAntNum*PilotFreqNum*PilotSymbNum
				for (k = PilotFreqNum - NoiseRow; k < PilotFreqNum; ++k) {
					NoiseVarEst += h_dct[k].real * h_dct[k].real + h_dct[k].imag * h_dct[k].imag;
				}

				//线性插值,复数的线性插值方法
				for (k = 0; k < PilotFreqNum; ++k) {
					CFR_est_nCh[PilotSymbIndex[p] * CarrierNum + PilotFreqIndex[k]].real = CFR_est_Pilot[k].real;
					CFR_est_nCh[PilotSymbIndex[p] * CarrierNum + PilotFreqIndex[k]].imag = CFR_est_Pilot[k].imag;
				}
				//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);
				//InterPolation_Freq(CFR_est_nCh, CarrierNum, PilotFreqIndex, SymbNum, PilotFreqNum);
				//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);
				//print_clx(CarrierNum, 4, "CFR_est_nCh", CFR_est_nCh);
			}

			//频域插值
			if (inter_freq != 1) {
				InterPolation_Freq(CFR_est_nCh, CarrierNum, PilotFreqIndex, SymbNum, PilotFreqNum);
			}

			//频域采用直接赋值的方式
			//if (inter_freq != 1) {
			//	Inter_Freq2(CFR_est_nCh, CarrierNum, PilotFreqIndex, PilotSymbIndex, PilotFreqNum, PilotSymbNum);
			//}

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
/*
			for (p = 0; p < SymbNum / 2; ++p) {
				for (k = 0; k < CarrierNum; ++k) {
					CFR_est_nCh[p * CarrierNum + k] = CFR_est_nCh[PilotSymbIndex[0] * CarrierNum + k];
				}
			}
			for (p = SymbNum / 2; p < SymbNum; ++p) {
				for (k = 0; k < CarrierNum; ++k) {
					CFR_est_nCh[p * CarrierNum + k] = CFR_est_nCh[PilotSymbIndex[1] * CarrierNum + k];		
				}
			}
			for (t = 0; t < CarrierNum*SymbNum; ++t) {
				CFR_est[t*RxAntNum*TxAntNum + j*RxAntNum + i] = CFR_est_nCh[t];
			}
//*/
			//print_clx(CarrierNum, SymbNum, "CFR_est_nCh", CFR_est_nCh);

			//H_est (RxAntNum,TxAntNum,CarrierNum,SymbNum)
//*	
			for (int m = 0; m < SymbNum; ++m) {
				if(m == 3 || m == 10){
					for (int n = 0; n < CarrierNum; ++n) {	
						CFR_est[(m * CarrierNum + n)*RxAntNum*TxAntNum + j*RxAntNum + i] = CFR_est_nCh[m * CarrierNum + n];
					}
				}
			}
//*/
		}
	}

	NoiseVarEst /= (RxAntNum * TxAntNum * NoiseRow * PilotSymbNum);

	free(PilotFreqIndex);
	free(SignalRecPilot);
	free(Pilot_nTx);
	free(CFR_est_Pilot);
	free(h_dct);
	free(CFR_est_nCh);
	
	return NoiseVarEst;
}

void Cal_DCT_FFT(lapack_complex_float* DCTMatrix, lapack_complex_float* Matrix, int M) {
	int j;
	lapack_complex_float *fft_1, *fft_2;
	DFTI_DESCRIPTOR_HANDLE my_desc1_handle;
	DftiCreateDescriptor(&my_desc1_handle, DFTI_SINGLE, DFTI_COMPLEX, 1, M);
	DftiCommitDescriptor(my_desc1_handle);

	fft_1 = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*M);
	fft_2 = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*M);
	if (M == 1) {
		DCTMatrix[0] = Matrix[0];
	}
	else {
		for (j = 0; j < M / 2; ++j) {
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

		DftiComputeForward(my_desc1_handle, fft_1);
		DftiComputeForward(my_desc1_handle, fft_2);

		DCTMatrix[0].real = sqrt(1 / (float)M) * (cos(pi * 0 / (2 * M)) * fft_1[0].real + sin(pi * 0 / (2 * M)) * fft_1[0].imag);
		DCTMatrix[0].imag = sqrt(1 / (float)M) * (cos(pi * 0 / (2 * M)) * fft_2[0].real + sin(pi * 0 / (2 * M)) * fft_2[0].imag);
		for (j = 1; j < M; ++j) {
			DCTMatrix[j].real = sqrt(2 / (float)M) * (cos(pi * j / (2 * M)) * fft_1[j].real + sin(pi * j / (2 * M)) * fft_1[j].imag);
			DCTMatrix[j].imag = sqrt(2 / (float)M) * (cos(pi * j / (2 * M)) * fft_2[j].real + sin(pi * j / (2 * M)) * fft_2[j].imag);
		}
	}
	DftiFreeDescriptor(&my_desc1_handle);
	free(fft_1);
	free(fft_2);
}

void Inter_Freq2(lapack_complex_float *CFR_est_nCh, int CarrierNum, int *PilotFreqIndex, int *PilotSymbIndex, int PilotFreqNum, int PilotSymbNum) {
	int c, t, k;
	int inter = CarrierNum / PilotFreqNum;
	for (k = 0; k < PilotSymbNum; ++k) {
		for (c = 0; c < PilotFreqNum; ++c) {
			for (t = 0; t < inter; ++t) {
				CFR_est_nCh[PilotSymbIndex[k] * CarrierNum + c * inter + t].real = CFR_est_nCh[PilotSymbIndex[k] * CarrierNum + PilotFreqIndex[c]].real;
				CFR_est_nCh[PilotSymbIndex[k] * CarrierNum + c * inter + t].imag = CFR_est_nCh[PilotSymbIndex[k] * CarrierNum + PilotFreqIndex[c]].imag;
			}
		}
	}
}
