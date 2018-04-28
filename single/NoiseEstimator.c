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

const float M_pi = 3.141592653;

void Cal_DCT_FFT(lapack_complex_float* DCTMatrix, lapack_complex_float* Matrix, int M);
float NoiseEstimator(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int PilotSymbNum, int inter_freq)
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

	NoiseVarEst = 0;
	PilotFreqNum = CarrierNum / inter_freq;
	PilotFreqIndex = (int*)malloc(sizeof(int)*PilotFreqNum);
	SignalRecPilot = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);
	Pilot_nTx = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	CFR_est_Pilot = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//
	h_dct = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*PilotFreqNum);//


	for (i = 0; i < RxAntNum; ++i) {
		for (j = 0; j < TxAntNum; ++j) {
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
				//print_clx(600, 1, "SignalRec_Pilot", SignalRecPilot);

				for (k = 0; k < PilotFreqNum; ++k) {
					// PilotSymb    (TxAntNum,PilotSymbNum)
					a = Pilot_nTx[k].real*Pilot_nTx[k].real + Pilot_nTx[k].imag*Pilot_nTx[k].imag;
					CFR_est_Pilot[k].real = (SignalRecPilot[k].real * Pilot_nTx[k].real + SignalRecPilot[k].imag * Pilot_nTx[k].imag) / a;
					CFR_est_Pilot[k].imag = (SignalRecPilot[k].imag * Pilot_nTx[k].real - SignalRecPilot[k].real * Pilot_nTx[k].imag) / a;
				}
				//print_clx(PilotFreqNum, 1, "CFR_est_Pilot", CFR_est_Pilot);

				//信道矩阵的DCT变换
				Cal_DCT_FFT(h_dct, CFR_est_Pilot, PilotFreqNum);
				//print_clx(PilotFreqNum, 1, "h_dct", h_dct);

				//获取信道相关矩阵
				//RxAntNum*TxAntNum*PilotFreqNum*PilotSymbNum
				for (k = PilotFreqNum - 2; k < PilotFreqNum; ++k) {
					NoiseVarEst += h_dct[k].real * h_dct[k].real + h_dct[k].imag * h_dct[k].imag;
				}

			}

		}
	}

	//NoiseVarEst = LAPACKE_zlange(LAPACK_COL_MAJOR, 'E', RxAntNum*TxAntNum, 2 * PilotSymbNum, CFR_Est, 2 * PilotSymbNum);
	//printf("%g\t", NoiseVarEst*NoiseVarEst / (RxAntNum * TxAntNum * 2 * PilotSymbNum));
	//NoiseVarEst = 0;

	NoiseVarEst /= (RxAntNum * TxAntNum * 2 * PilotSymbNum);
	//printf("NoiseVar = %g\t", NoiseVarEst);
	free(PilotFreqIndex);
	free(SignalRecPilot);
	free(Pilot_nTx);
	free(CFR_est_Pilot);
	free(h_dct);

	return NoiseVarEst;
}

