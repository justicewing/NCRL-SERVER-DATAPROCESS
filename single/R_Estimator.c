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
#include <string.h>
#include "mkl.h"


void Cal_DCT_FFT(lapack_complex_float* DCTMatrix, lapack_complex_float* Matrix, int M);
void InterPolation(float *R_est, int CarrierNum, int *ColIndex, int SymbNum, int PilotSymbNum);

float REstimator(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int SymbNum, int PilotSymbNum, int inter_freq, float *R_est)
{
	//inter_freq 代表在频域放置导频的位置
	int i, j, p, k;
	float a;
	int PilotFreqNum;
	int *PilotFreqIndex;
	float NoiseVarEst;

	lapack_complex_float *SignalRecPilot;
	lapack_complex_float *Pilot_nTx;
	lapack_complex_float *CFR_est_Pilot;
	lapack_complex_float *h_dct;
	float *R_tmp;
	//lapack_complex_float* CFR_Est;

	NoiseVarEst = 0.0;
	PilotFreqNum = CarrierNum / inter_freq;
	PilotFreqIndex = (int*)malloc(sizeof(int)*PilotFreqNum);
	SignalRecPilot = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);
	Pilot_nTx = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	CFR_est_Pilot = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	h_dct = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	R_tmp = (float*)malloc(sizeof(float) * PilotFreqNum * SymbNum);

	memset(R_est, 0, sizeof(R_est));
	//print_arr(PilotFreqNum, 5, "DCT_Mx", DCT_Mx);

	for (i = 0; i < RxAntNum; ++i) {
		for (j = 0; j < TxAntNum; ++j) {
			//信道矩阵的初始化
			memset(R_tmp, 0, sizeof(R_tmp));
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
					Pilot_nTx[k] = PilotSymb[p * CarrierNum*TxAntNum + PilotFreqIndex[k] * TxAntNum + j];
				}
				//print_clx(PilotFreqNum, 1, "Pilot_nTx", Pilot_nTx);
				//获取接收信号
				for (k = 0; k < PilotFreqNum; ++k) {
					//signalRec_R(RxAntNum, CarrierNum, PilotSymbNum)
					SignalRecPilot[k] = SignalRec[PilotSymbIndex[p] * CarrierNum*RxAntNum + PilotFreqIndex[k] * RxAntNum + i];
				}
				//print_clx(PilotFreqNum, 1, "SignalRec_Pilot", SignalRecPilot);

				for (k = 0; k < PilotFreqNum; ++k) {
					// PilotSymb    (TxAntNum,PilotSymbNum)
					a = Pilot_nTx[k].real*Pilot_nTx[k].real + Pilot_nTx[k].imag*Pilot_nTx[k].imag;
					CFR_est_Pilot[k].real = (SignalRecPilot[k].real * Pilot_nTx[k].real + SignalRecPilot[k].imag * Pilot_nTx[k].imag) / a;
					CFR_est_Pilot[k].imag = (SignalRecPilot[k].imag * Pilot_nTx[k].real - SignalRecPilot[k].real * Pilot_nTx[k].imag) / a;
				}
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//信道矩阵的DCT变换
				Cal_DCT_FFT(h_dct, CFR_est_Pilot, PilotFreqNum);
				//print_clx(150, 1, "h_dct", h_dct);

				//获取信道相关矩阵
				//R_CFR(RxAntNum, TxAntNum, PilotFreqNum, PilotSymbNum)
				for (k = 0; k < PilotFreqNum; ++k) {
					//CFR_Est[p * RxAntNum*TxAntNum * 2 + k * RxAntNum * TxAntNum + j * RxAntNum + i].real = h_dct[PilotFreqNum - 2 + k].real;
					//CFR_Est[p * RxAntNum*TxAntNum * 2 + k * RxAntNum * TxAntNum + j * RxAntNum + i].imag = h_dct[PilotFreqNum - 2 + k].imag;
					R_tmp[PilotSymbIndex[p] * PilotFreqNum + k] = h_dct[k].real * h_dct[k].real + h_dct[k].imag * h_dct[k].imag;
				}
				for (k = PilotFreqNum - 2; k < PilotFreqNum; ++k) {
					NoiseVarEst += R_tmp[PilotSymbIndex[p] * PilotFreqNum + k];
				}
			}
			//时域的插值
			InterPolation(R_tmp, PilotFreqNum, PilotSymbIndex, SymbNum, PilotSymbNum);
			
			/*
			for (p = 0; p < SymbNum / 2; ++p) {
				for (k = 0; k < CarrierNum; ++k) {
					R_tmp[p * PilotFreqNum + k] = R_tmp[PilotSymbIndex[0] * PilotFreqNum + k];
				}
			}
			for (p = SymbNum / 2; p < SymbNum; ++p) {
				for (k = 0; k < CarrierNum; ++k) {
					R_tmp[p * PilotFreqNum + k] = R_tmp[PilotSymbIndex[1] * PilotFreqNum + k];
				}
			}
			*/
			
			//R_est(RxAntNum, TxAntNum, PilotFreqNum, SymbNum)
			for (p = 0; p < SymbNum; ++p) {
				//如果高频成分直接置零
				for (k = 0; k < PilotFreqNum / 4; ++k) {
					R_est[p * PilotFreqNum * RxAntNum * TxAntNum + k * RxAntNum * TxAntNum + j * RxAntNum + i] = R_tmp[p * PilotFreqNum + k];
				}
			}
		}
	}

	//NoiseVarEst = LAPACKE_zlange(LAPACK_COL_MAJOR, 'E', RxAntNum*TxAntNum, 2 * PilotSymbNum, CFR_Est, 2 * PilotSymbNum);
	//printf("%g\t", NoiseVarEst*NoiseVarEst / (RxAntNum * TxAntNum * 2 * PilotSymbNum));
	//NoiseVarEst = 0;
	NoiseVarEst /= (RxAntNum * TxAntNum * 2 * PilotSymbNum);
	//printf("NoiseVar = %g\t", NoiseVarEst);

	//信道相关阵的处理？
	//高频成分直接置零？？
	for (i = 0; i < RxAntNum; ++i) {
		for (j = 0; j < TxAntNum; ++j) {
			for (k = 0; k < PilotFreqNum / 4; ++k) {
				for (p = 0; p < SymbNum; ++p) {
					R_est[p * RxAntNum * TxAntNum * PilotFreqNum + k * RxAntNum * TxAntNum + j * RxAntNum + i] -= NoiseVarEst;
				}
			}
		}
	}
	//print_arr(RxAntNum, TxAntNum, "R_est", R_est + 24*1200);

	free(PilotFreqIndex);
	free(SignalRecPilot);
	free(Pilot_nTx);
	free(CFR_est_Pilot);
	free(R_tmp);
	free(h_dct);

	return NoiseVarEst;
}

void InterPolation(float *R_est, int CarrierNum, int *ColIndex, int SymbNum, int PilotSymbNum) {
	int c, t, k;
	int a;
	float b;
	for (k = 0; k < CarrierNum; ++k) {
		a = ColIndex[1] - ColIndex[0];
		b = (R_est[ColIndex[1] * CarrierNum + k] - R_est[ColIndex[0] * CarrierNum + k]) / a;
		for (c = 0; c < ColIndex[0]; ++c) {
			R_est[c*CarrierNum + k] = R_est[ColIndex[0] * CarrierNum + k] - (ColIndex[0] - c) * b;
		}

		for (t = 0; t < PilotSymbNum - 1; ++t) {
			a = ColIndex[t + 1] - ColIndex[t];
			b = (R_est[ColIndex[t + 1] * CarrierNum + k] - R_est[ColIndex[t] * CarrierNum + k]) / a;
			for (c = ColIndex[t] + 1; c < ColIndex[t + 1]; ++c) {
				R_est[c*CarrierNum + k] = R_est[(c - 1) * CarrierNum + k] + b;
			}

		}
		//a = ColIndex[PilotSymbNum - 1] - ColIndex[PilotSymbNum - 2];
		//b_real = (CFR_est_nCh[ColIndex[PilotSymbNum - 1] * CarrierNum + k].real - CFR_est_nCh[ColIndex[PilotSymbNum - 2] * CarrierNum + k].real) / a;
		//b_imag = (CFR_est_nCh[ColIndex[PilotSymbNum - 1] * CarrierNum + k].imag - CFR_est_nCh[ColIndex[PilotSymbNum - 2] * CarrierNum + k].imag) / a;
		for (c = ColIndex[PilotSymbNum - 1] + 1; c < SymbNum; ++c) {
			R_est[c*CarrierNum + k] = R_est[(c - 1) * CarrierNum + k] + b;
		}
	}

}


