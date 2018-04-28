//CQI和链路自适应模块
//输入参数：
//UserCh,UserCh:(RxAntNum,TxAntNum,CarrierNum,SymbolNum)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mkl.h"
float *exp_table;
float *log_ln_table;
float *beta_log_table;

const float ex = 2.718281828;
float CQI_mapping_table[15] = { 2, 3, 4, 5.5, 7, 8, 12, 13.5, 14.5, 16.5, 18, 19.5, 21.5, 23, 27 };
float beta_table[15] = { 1.58, 1.48, 1.45, 1.36, 1.57, 1.5, 3.61, 4.75, 6.25, 11.84, 16.32, 24.39, 29.1, 31.82, 35.11 };

void RxAdptLink(lapack_complex_float *UserCh, float *SymbVarEst, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int SymbolNum, int *CQI_feedback)
{
	int i, j, k, s;
	float tmp;
	float *UserSINR;
	float *SINR_eff;
	lapack_complex_float *TmpCh;
	lapack_complex_float *TmpQ;
	lapack_int *ipiv;

	TmpCh = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*RxAntNum*TxAntNum);
	TmpQ = (lapack_complex_float*)malloc(sizeof(lapack_complex_float)*TxAntNum*TxAntNum);
	ipiv = (lapack_int*)malloc(sizeof(lapack_int)*TxAntNum);
	UserSINR = (float*)malloc(sizeof(float)*TxAntNum*CarrierNum*SymbolNum);
	SINR_eff = (float*)malloc(sizeof(float)*TxAntNum * 15);
	float mean = 0;
	//指数等效信噪比映射
	for (i = 0; i < TxAntNum; ++i) {
		for (j = 0; j < 15; ++j) {
			SINR_eff[j*TxAntNum + i] = 0;

			for (k = 0; k < CarrierNum; ++k) {
				for (s = 0; s < SymbolNum; ++s) {
					tmp = SymbVarEst[s*CarrierNum*TxAntNum + k*TxAntNum + i] / beta_table[j];
					SINR_eff[j*TxAntNum + i] += pow(ex, -tmp);
				}
			}
			//printf("%g\n", SINR_eff[j*TxAntNum + i]);
			SINR_eff[j*TxAntNum + i] = - beta_table[j] * log(SINR_eff[j*TxAntNum + i] / (CarrierNum * SymbolNum));
			//printf("%g\n", SINR_eff[j*TxAntNum + i]);
			SINR_eff[j*TxAntNum + i] = 10 * log10(SINR_eff[j*TxAntNum + i]);
			//printf("%g ", SINR_eff[j*TxAntNum + i]);
		}
	}
	//printf("\n");
	//CQI_feedback (1,TxAntNum)
	for (i = 0; i < TxAntNum; ++i) {
		CQI_feedback[i] = 1;
		for (j = 14; j >= 0; --j) {
			if (SINR_eff[j*TxAntNum + i] > CQI_mapping_table[j]) {
				CQI_feedback[i] = j + 1;
				break;
			}
		}
		//printf("%lf\t", SINR_eff[j*TxAntNum + i]);
		//printf("%lf\t", -10 * log10(1 / mean));
	}
	//for (i = 0; i < TxAntNum; ++i) {
	//	printf("%d\t", CQI_feedback[i]);
	//}
	
	free(UserSINR);
	free(SINR_eff);

}

void genAdptLinktable(){
	int ExpNum = 10000;
	float Exp_max = 10;
	int LogLnNum = 9999;
	float LogLn_max = 1;
	float step;
	exp_table = (float *) malloc(sizeof(float) * ExpNum);
	log_ln_table = (float *) malloc(sizeof(float) * LogLnNum);
	beta_log_table = (float *) malloc(sizeof(float) * 15);
	
	//y = exp(-x)   x~(0.001,10) y~(1,0)
	step = Exp_max / ExpNum;
	//printf("\nexp_table:\n");
	for(int i = 0; i < ExpNum; i++){
		exp_table[i] = pow(ex, -(i * step));
		//printf("%.4f : %.4f ",-(i * step),exp_table[i]);
	}
	
	//y = 10 * lg(-ln(x))   x~(0.0001,0.9999) y~(10,-40)
	step = LogLn_max / (LogLnNum + 1);
	//printf("\nlog_ln_table:\n");
	for(int i = 0; i < LogLnNum; i++){
		log_ln_table[i] = 10 * log10(-log(i * step + step));
		//printf("%.4f : %.4f ",i * step + step,log_ln_table[i]);
	}
	
	//y = 10 * lg(beta)
	//printf("\nbeta_log_table:\n");
	for(int i = 0; i < 15; i++){
		beta_log_table[i] = 10 * log10(beta_table[i]);
		//printf("%d : %.4f ",i,beta_log_table[i]);
	}
}

void RxAdptLink_lut(lapack_complex_float *UserCh, float *SymbVarEst, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int SymbolNum, int *CQI_feedback)
{
	int i, j, k, s;
	float tmp;
	float *UserSINR;
	float *SINR_eff;
	float ttt;
	
	UserSINR = (float*)malloc(sizeof(float)*TxAntNum*CarrierNum*SymbolNum);
	SINR_eff = (float*)malloc(sizeof(float)*TxAntNum * 15);
	
	//指数等效信噪比映射
	for (i = 0; i < TxAntNum; ++i) {
		for (j = 0; j < 15; ++j) {
			SINR_eff[j*TxAntNum + i] = 0;
			ttt = 0;
			for (k = 0; k < CarrierNum; ++k) {
				for (s = 0; s < SymbolNum; ++s) {
					tmp = SymbVarEst[s*CarrierNum*TxAntNum + k*TxAntNum + i] / beta_table[j];
					//printf("%f ",tmp);
					if(tmp < 10) ttt += exp_table[(int)(tmp / 0.001)];
				}
			}
			//printf("%g\n", ttt);
			SINR_eff[j * TxAntNum + i] = beta_log_table[j] + log_ln_table[(int)(ttt / CarrierNum / SymbolNum / 0.0001)];
			//printf("%g ", SINR_eff[j*TxAntNum + i]);
		}
	}
	//printf("\n");
	for (i = 0; i < TxAntNum; ++i) {
		CQI_feedback[i] = 1;
		for (j = 14; j >= 0; --j) {
			if (SINR_eff[j*TxAntNum + i] > CQI_mapping_table[j]) {
				CQI_feedback[i] = j + 1;
				break;
			}
		}
	}
	free(UserSINR);
	free(SINR_eff);

}

void freeAdptLinktable(){
	free(exp_table);
	free(log_ln_table);
	free(beta_log_table);
}
