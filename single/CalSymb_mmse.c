//Input:
//    1. the channel matrix: 4-D matrix(if SymbolNum is nonzero), complex, (RxAntNum, LayerNum, UserCarrNum, SymbolNum)
//	  2. the received signal: 3-D matrix(if SymbolNum is nonzero), complex, (LayerNum, UserCarrNum, SymbolNum)
//	  3. the antenna number of the receiver, float 
//	  4. the number of the subcarrier,float
//	  5. the number of the layer, float
//	  6. the number of the symbol, float
//    7. the Standard Deviation of the signal, float
//    8. the number of the subcarrier to calculate the average channel matirx, float
//    9. average the channel matrix or not, 0-no average,1-average
//
//输出：
//	  1. the estimated symbol matrix： 3-D matrix, complex, （nLayer, nCarrier, nSymb）
//	  2. the variance matrix : 3-D matirx, float, （nLayer, nCarrier, nSymb）

//To call the mex function:
//load the channel matrix
//link the static library and compile the file: mex  CalSymb_mmse.c mkl_core.lib mkl_intel_thread.lib mkl_intel_lp64.lib libiomp5md.lib
//run the mex file: [SymbMeanEst,SymbVarEst]=CalSymb_mmse(h_tmp,SignalRec,RxAntNum,CarrierNum,LayerNum,SymbNum,Sigma,Step,flg_ave);

#include <stdio.h>
#include <stdlib.h>
#include "mkl.h"

//单流
void CalSymb_mmse_1(lapack_complex_float *h_tmp, lapack_complex_float *Signal, int RxAntNum, int CarrierNum, int SymbNum, float sigma, int Step, int flg_ave, lapack_complex_float *SymbEst, float *SymbVar, float *SINR_est)
{
	int i, j, k;
	lapack_complex_float x_est;
	float x_var = 0;
	lapack_complex_float *ch_mx;
	lapack_complex_float *Signal_Rec;
	float y_temp = 0;
	float scale = 4;
	int LayerNum = 1;
	Signal_Rec = (lapack_complex_float *)malloc(sizeof(lapack_complex_float)*RxAntNum);
	ch_mx = (lapack_complex_float *)malloc(sizeof(lapack_complex_float)*RxAntNum*LayerNum);
	//the estimation of every symbol and its var, every subcarrier 
	//
	if (!flg_ave) Step = 1;
	int H_map[14] = {3,3,3,3,3,3,3,10,10,10,10,10,10,10};
	for (i = 0; i < SymbNum; ++i) {
		for (j = 0; j < CarrierNum; ++j) {
			//获取信道信息
			x_var = 0;
			x_est.real = 0;
			x_est.imag = 0;
			//
			//获取接收信号
			ch_mx = &h_tmp[H_map[i] * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum];
			//ch_mx = &h_tmp[i * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum];
			Signal_Rec = &Signal[i * CarrierNum * RxAntNum + j * RxAntNum];
			//for (k = 0; k < RxAntNum; ++k) {
			//	ch_mx[k] = h_tmp[i * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum + k];
			//	Signal_Rec[k] = Signal[i * CarrierNum * RxAntNum + j * RxAntNum + k];
			//}
			//print_mx(ch_mx, "ch_mx", RxAntNum, LayerNum);
			//print_mx(Signal_Rec, "Signal_Rec", RxAntNum, 1);

			for (k = 0; k < RxAntNum; ++k) {
				y_temp = 0;
				y_temp += (Signal_Rec[k].real * Signal_Rec[k].real + Signal_Rec[k].imag * Signal_Rec[k].imag);
				x_var += (ch_mx[k].real * ch_mx[k].real + ch_mx[k].imag * ch_mx[k].imag);
				x_est.real += (ch_mx[k].real * Signal_Rec[k].real + ch_mx[k].imag * Signal_Rec[k].imag);
				x_est.imag += (ch_mx[k].real * Signal_Rec[k].imag - ch_mx[k].imag * Signal_Rec[k].real);
			}
			//printf("%g\t", x_var);
			SymbEst[i * CarrierNum * LayerNum + j * LayerNum].real = x_est.real / x_var;
			SymbEst[i * CarrierNum * LayerNum + j * LayerNum].imag = x_est.imag / x_var;
			//if(i ==1 && j == 235 ) printf("%g\t",  sigma * sigma / x_var);
			x_var = sigma * sigma / x_var;// + scale * (sigma * sigma) * (sigma * sigma) / y_temp / CarrierNum;
			SINR_est[i * CarrierNum * LayerNum + j * LayerNum] = 1 / (x_var + scale * (sigma * sigma) * (sigma * sigma) / y_temp / CarrierNum);
			//if(i ==1 && j == 235 ) printf("%g\t", x_var);
			//printf("%g\t", x_var);
			SymbVar[i * CarrierNum * LayerNum + j * LayerNum] = x_var;
			//printf("(%g + i* %g)", x_est.real, x_est.imag);
		}
	}
}

//多流
void init_SignalRec(lapack_complex_float *Signal, int nCarrier, int nSymb, int RxAntNum, int CarrierNum, lapack_complex_float *Signal_Rec);
void CalSymbEst(lapack_complex_float *Signal_Rec, float sigma, int RxAntNum, int TxAntNum, lapack_complex_float *ch_q1, lapack_complex_float *ch_q2, float *lamda, lapack_complex_float *x_est, float *x_var);
void CalSymbEst2(lapack_complex_float *Signal_Rec, float sigma, int RxAntNum, int LayerNum, lapack_complex_float* Q_Mx, float *lamda, lapack_complex_float *x_est, float *x_var);
void qr_dcpt(lapack_complex_float *ch, lapack_complex_float *tau, float sigma, int RxAntNum, int TxAntNum, lapack_complex_float *mx_q);
void cal_ExtendedCh(lapack_complex_float *ch, float sigma, int RxAntNum, int TxAntNum, lapack_complex_float *ch_ex);
void Cal_ChQ(lapack_complex_float *Q, int RxAntNum, int TxAntNum, lapack_complex_float *q1, lapack_complex_float *q2, float *lamda, lapack_complex_float* Q_Mx);
void Cal_LamdaMx(lapack_complex_float *q2, float *lamda, int TxAntNum);


void CalSymb_mmse(lapack_complex_float *h_tmp, lapack_complex_float *Signal, int RxAntNum, int CarrierNum, int LayerNum, int SymbNum, float sigma, int Step, int flg_ave, lapack_complex_float *SymbEst, float *SymbVar, float *SINR_est)
{
	int i, j, k, s;
	lapack_complex_float *x_est;
	float *x_var;
	lapack_complex_float *ch_mx;
	lapack_complex_float *ch_res;
	lapack_complex_float *Signal_Rec;

	//the variable used in the subfunction
	lapack_complex_float *ch_q;
	lapack_complex_float *ch_q1, *ch_q2;
	lapack_complex_float *Q_Matrix;
	float *lamda;
	lapack_complex_float *tau;

	float scale = 1;
	float y_temp = 0;

	x_est = (lapack_complex_float *)malloc(sizeof(lapack_complex_float)*LayerNum);
	x_var = (float *)malloc(sizeof(float)*LayerNum);
	Signal_Rec = (lapack_complex_float *)malloc(sizeof(lapack_complex_float)*RxAntNum);
	ch_mx = (lapack_complex_float *)malloc(sizeof(lapack_complex_float)*SymbNum*CarrierNum*RxAntNum*LayerNum);
	//ch_res = (lapack_complex_float *)malloc(sizeof(lapack_complex_float)*RxAntNum*LayerNum);

	ch_q = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*(RxAntNum + LayerNum)*LayerNum);
	ch_q1 = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*RxAntNum*LayerNum);
	ch_q2 = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*LayerNum*LayerNum);
	Q_Matrix = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*LayerNum*RxAntNum);
	lamda = (float*)malloc(sizeof(float)*LayerNum);
	tau = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*LayerNum);

	if (!flg_ave) Step = 1;
	int H_map[2] = {3,10};
	//the estimation of every symbol and its var, every subcarrier 
	for (s = 0; s < 2; ++s){
		for (j = 0; j < CarrierNum; ++j) {
			for(i = 0; i < SymbNum / 2; ++i){
				
				if (!(j % Step) && i == 0) {
					ch_res = h_tmp + H_map[s] * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum;
					/*
					for (k = 0; k < RxAntNum * LayerNum; ++k) {
						//ch_res[k].real = h_tmp[H_map[s] * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum + k].real;
						//ch_res[k].imag = h_tmp[H_map[s] * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum + k].imag;
						ch_res[k].real = h_tmp[(s * SymbNum / 2 + i) * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum + k].real;
						ch_res[k].imag = h_tmp[(s * SymbNum / 2 + i) * CarrierNum * RxAntNum * LayerNum + j * RxAntNum * LayerNum + k].imag;
					}
					*/
					//calculate the extended channel matrix and its Q matrix
					qr_dcpt(ch_res, tau, sigma, RxAntNum, LayerNum, ch_q);
					Cal_ChQ(ch_q, RxAntNum, LayerNum, ch_q1, ch_q2, lamda, Q_Matrix);
				}

				init_SignalRec(Signal, j, (s * SymbNum / 2 + i), RxAntNum, CarrierNum, Signal_Rec);

				y_temp = 0;
				for (int c = 0; c < RxAntNum; ++c) {
					y_temp += (Signal_Rec[c].real * Signal_Rec[c].real + Signal_Rec[c].imag * Signal_Rec[c].imag);
				}

				//CalSymbEst(Signal_Rec, sigma, RxAntNum, LayerNum, ch_q1, ch_q2, lamda, x_est, x_var);
				CalSymbEst2(Signal_Rec, sigma, RxAntNum, LayerNum, Q_Matrix, lamda, x_est, x_var);
				for (k = 0; k < LayerNum; ++k){
					SymbEst[((s * SymbNum / 2 + i) * CarrierNum + j) * LayerNum + k].real = x_est[k].real;
					SymbEst[((s * SymbNum / 2 + i) * CarrierNum + j) * LayerNum + k].imag = x_est[k].imag;
					//if(j == 0) printf("\nlayer %d x_est :%f+%fi",k,x_est[k].real, x_est[k].imag);
					SymbVar[((s * SymbNum / 2 + i) * CarrierNum + j) * LayerNum + k] = x_var[k];
					SINR_est[((s * SymbNum / 2 + i) * CarrierNum + j) * LayerNum + k] = 1 / (x_var[k] + scale * (sigma * sigma) * (sigma * sigma) / y_temp / CarrierNum);
				}
			}
		}
	}

	free(x_est);
	free(x_var);
	free(ch_mx);
	free(Signal_Rec);
	//free(ch_res);
	//the variable used in the subfunction
	free(ch_q);
	free(ch_q1);
	free(ch_q2);
	free(Q_Matrix);
	free(lamda);
	free(tau);
}


void init_SignalRec(lapack_complex_float *Signal, int nCarrier, int nSymb, int RxAntNum, int CarrierNum, lapack_complex_float *Signal_Rec)
{
	int i;
	for (i = 0; i < RxAntNum; ++i) {
		Signal_Rec[i].real = Signal[(nSymb*CarrierNum + nCarrier)*RxAntNum + i].real;
		Signal_Rec[i].imag = Signal[(nSymb*CarrierNum + nCarrier)*RxAntNum + i].imag;
	}
}

void CalSymbEst(lapack_complex_float *Signal_Rec, float sigma, int RxAntNum, int LayerNum, lapack_complex_float *ch_q1, lapack_complex_float *ch_q2, float *lamda, lapack_complex_float *x_est, float *x_var)
{
	int i, j, k;
	lapack_complex_float temp1, temp2;

	//method one
	for (i = 0; i < LayerNum; ++i) {
		temp2.real = 0;
		temp2.imag = 0;
		for (j = 0; j < RxAntNum; ++j) {
			temp1.real = 0;
			temp1.imag = 0;
			for (k = 0; k < LayerNum; ++k) {
				temp1.real = temp1.real + ch_q2[k*LayerNum + i].real*ch_q1[k*RxAntNum + j].real + ch_q2[k*LayerNum + i].imag*ch_q1[k*RxAntNum + j].imag;
				temp1.imag = temp1.imag + ch_q2[k*LayerNum + i].imag*ch_q1[k*RxAntNum + j].real - ch_q2[k*LayerNum + i].real*ch_q1[k*RxAntNum + j].imag;
			}
			temp2.real = temp2.real + temp1.real * Signal_Rec[j].real - temp1.imag * Signal_Rec[j].imag;
			temp2.imag = temp2.imag + temp1.real * Signal_Rec[j].imag + temp1.imag * Signal_Rec[j].real;
		}
		x_est[i].real = (1 / lamda[i]) * (1 / sigma) * temp2.real;
		x_est[i].imag = (1 / lamda[i]) * (1 / sigma) * temp2.imag;
		x_var[i] = (1 / lamda[i]) - 1;
	}

}

void CalSymbEst2(lapack_complex_float *Signal_Rec, float sigma, int RxAntNum, int LayerNum, lapack_complex_float* Q_Mx, float *lamda, lapack_complex_float *x_est, float *x_var){
	int i, j;
	lapack_complex_float temp2;

	//method one
	for (i = 0; i < LayerNum; ++i) {
		temp2.real = 0;
		temp2.imag = 0;
		for (j = 0; j < RxAntNum; ++j) {
			temp2.real = temp2.real + Q_Mx[j * LayerNum + i].real * Signal_Rec[j].real - Q_Mx[j * LayerNum + i].imag * Signal_Rec[j].imag;
			temp2.imag = temp2.imag + Q_Mx[j * LayerNum + i].real * Signal_Rec[j].imag + Q_Mx[j * LayerNum + i].imag * Signal_Rec[j].real;
		}
		x_est[i].real = (1 / lamda[i]) * (1 / sigma) * temp2.real;
		x_est[i].imag = (1 / lamda[i]) * (1 / sigma) * temp2.imag;
		x_var[i] = (1 / lamda[i]) - 1;
	}

}


void qr_dcpt(lapack_complex_float *ch, lapack_complex_float *tau, float sigma, int RxAntNum, int LayerNum, lapack_complex_float *mx_q) {

	cal_ExtendedCh(ch, sigma, RxAntNum, LayerNum, mx_q);
	LAPACKE_cgeqrf(LAPACK_COL_MAJOR, RxAntNum + LayerNum, LayerNum, mx_q, (RxAntNum + LayerNum), tau);
	LAPACKE_cungqr(LAPACK_COL_MAJOR, RxAntNum + LayerNum, LayerNum, LayerNum, mx_q, (RxAntNum + LayerNum), tau);
}


void cal_ExtendedCh(lapack_complex_float *ch, float sigma, int RxAntNum, int LayerNum, lapack_complex_float *ch_ex) {
	int i, j;
	for (i = 0; i < LayerNum; ++i) {
		for (j = 0; j < (RxAntNum + LayerNum); ++j) {
			if (j < RxAntNum)
				ch_ex[i*(RxAntNum + LayerNum) + j] = ch[i*RxAntNum + j];
			else if (i == j - RxAntNum) {
				ch_ex[i*(RxAntNum + LayerNum) + j].real = sigma;
				ch_ex[i*(RxAntNum + LayerNum) + j].imag = 0;
			}
			else {
				ch_ex[i*(RxAntNum + LayerNum) + j].real = 0;
				ch_ex[i*(RxAntNum + LayerNum) + j].imag = 0;
			}
		}
	}
}



void Cal_ChQ(lapack_complex_float *Q, int RxAntNum, int LayerNum, lapack_complex_float *q1, lapack_complex_float *q2, float *lamda, lapack_complex_float* Q_Mx) {
	int i, j, k;
	float temp = 0;
	lapack_complex_float temp2;

	for (i = 0; i < LayerNum; ++i) {
		for (j = 0; j < (RxAntNum + LayerNum); ++j) {
			if (j < RxAntNum)
				q1[i*RxAntNum + j] = Q[i*(RxAntNum + LayerNum) + j];
			else
				q2[i*LayerNum + j - RxAntNum] = Q[i*(RxAntNum + LayerNum) + j];
		}
	}

	for (i = 0; i < LayerNum; ++i) {
		temp = 0;
		for (j = 0; j < LayerNum; ++j) {
			temp = temp + q2[j*LayerNum + i].real*q2[j*LayerNum + i].real + q2[j*LayerNum + i].imag*q2[j*LayerNum + i].imag;
		}
		lamda[i] = 1.0f - temp;
	}
	//cal Q2*Q1'
	for(i = 0; i < LayerNum; ++i){
		for(j = 0; j < RxAntNum; ++j){
			temp2.real = 0;
			temp2.imag = 0;
			for(k = 0; k < LayerNum; ++k){
				temp2.real += q2[k*LayerNum + i].real * q1[k*RxAntNum + j].real + q2[k*LayerNum + i].imag * q1[k*RxAntNum + j].imag;
				temp2.imag += q2[k*LayerNum + i].imag * q1[k*RxAntNum + j].real - q2[k*LayerNum + i].real * q1[k*RxAntNum + j].imag;
			}
			Q_Mx[j * LayerNum + i].real = temp2.real;
			Q_Mx[j * LayerNum + i].imag = temp2.imag;
		}
	}

}

void Cal_LamdaMx(lapack_complex_float *q2, float *lamda, int LayerNum) {
	int i, j;
	float temp;
	for (i = 0; i < LayerNum; ++i) {
		temp = 0;
		for (j = 0; j < LayerNum; ++j) {
			temp = temp + q2[j*LayerNum + i].real*q2[j*LayerNum + i].real + q2[j*LayerNum + i].imag*q2[j*LayerNum + i].imag;
		}
		lamda[i] = 1.0f - temp;
	}
}

