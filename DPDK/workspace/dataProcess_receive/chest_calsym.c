#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <complex.h>
#include <math.h>
#include "mkl.h"
#include "chest_calsym.h"

extern pthread_mutex_t mutex1_rx;

void chest_calsym(void *arg){

	struct chest_calsym_args_t chest_calsym_args = *((struct chest_calsym_args_t *)arg);
	int m = chest_calsym_args.SBindex;
	int k = 0, j = 0;
	/*
	for(int ns = 0; ns < chest_calsym_args.SymbNum; ns++){
		for(int nc = chest_calsym_args.CarrierNum_SB * m; nc < chest_calsym_args.CarrierNum_SB * m + chest_calsym_args.CarrierNum_SB; nc++){
			for(int nr = 0; nr < chest_calsym_args.RxAntNum; nr++){
				for(int nl = 0; nl < chest_calsym_args.LayerNum; nl++){
					j = chest_calsym_args.CarrierNum * chest_calsym_args.RxAntNum * chest_calsym_args.LayerNum * ns + chest_calsym_args.RxAntNum * chest_calsym_args.LayerNum * nc + chest_calsym_args.LayerNum * nr + nl;
					chest_calsym_args.CFR_SB[k] = chest_calsym_args.CFR[j];
					//printf("%f+%fi ",chest_calsym_args.CFR_SB[k].real,chest_calsym_args.CFR_SB[k].imag);
					k++;
				}
			}
		}
	}
	*/
	//printf("\nk=%d,j=%d",k,j);
	//sleep(1);
	k = 0;
	for(int ns = 0; ns < chest_calsym_args.SymbNum; ns++){
		for(int nc = chest_calsym_args.CarrierNum_SB * m; nc < chest_calsym_args.CarrierNum_SB * m + chest_calsym_args.CarrierNum_SB; nc++){
			for(int i = 0; i < chest_calsym_args.RxAntNum; i++){
				j = chest_calsym_args.CarrierNum * chest_calsym_args.RxAntNum * ns + chest_calsym_args.RxAntNum * nc + i;
				chest_calsym_args.SignalRec[k] = chest_calsym_args.package[j];
				k++;
			}
		}
	}
	k = 0;
	for(int ns = 0; ns < chest_calsym_args.PilotSymbNum; ns++){
		for(int nc = chest_calsym_args.CarrierNum_SB * m; nc < chest_calsym_args.CarrierNum_SB * m + chest_calsym_args.CarrierNum_SB; nc++){
			for(int i = 0; i < chest_calsym_args.LayerNum; i++){
				j = chest_calsym_args.CarrierNum * chest_calsym_args.LayerNum * ns + chest_calsym_args.LayerNum * nc + i;
				chest_calsym_args.PilotSymb_SB[k] = chest_calsym_args.PilotSymb[j];
				k++;
			}
		}
	}
	
	float sigma2 = 0;
	
	//Channel estimation
	if(chest_calsym_args.ChEstType == 1){
		sigma2 = REstimator(chest_calsym_args.SignalRec, chest_calsym_args.PilotSymb_SB, chest_calsym_args.PilotSymbIndex, chest_calsym_args.LayerNum, chest_calsym_args.RxAntNum, chest_calsym_args.CarrierNum_SB, chest_calsym_args.SymbNum, chest_calsym_args.PilotSymbNum, chest_calsym_args.inter_freq, chest_calsym_args.R_DCT, chest_calsym_args.Rest_p);
		ChannelEstimator_DCT(chest_calsym_args.SignalRec, chest_calsym_args.PilotSymb_SB, chest_calsym_args.R_DCT, chest_calsym_args.PilotSymbIndex, chest_calsym_args.LayerNum, chest_calsym_args.RxAntNum, chest_calsym_args.CarrierNum_SB, chest_calsym_args.UserNum, chest_calsym_args.LayerNum, chest_calsym_args.PilotSymbNum, chest_calsym_args.SymbNum, chest_calsym_args.inter_freq, sqrt(sigma2), chest_calsym_args.CFR_est, chest_calsym_args.chestDCT_p);
	}
	else{
		sigma2 = ChannelEstimator_LS(chest_calsym_args.SignalRec, chest_calsym_args.PilotSymb_SB, chest_calsym_args.PilotSymbIndex, chest_calsym_args.LayerNum, chest_calsym_args.RxAntNum, chest_calsym_args.CarrierNum_SB, chest_calsym_args.PilotSymbNum, chest_calsym_args.SymbNum, chest_calsym_args.inter_freq, chest_calsym_args.CFR_est, chest_calsym_args.chestLS_p);
	}
	//sleep(10);
	//calculate symbol
	if(chest_calsym_args.LayerNum == 1){
		CalSymb_mmse_1(chest_calsym_args.CFR_est, chest_calsym_args.SignalRec, chest_calsym_args.RxAntNum, chest_calsym_args.CarrierNum_SB, chest_calsym_args.SymbNum, sqrt(sigma2), chest_calsym_args.Step, chest_calsym_args.flg_ave, chest_calsym_args.SymbEst, chest_calsym_args.SymbVar, chest_calsym_args.SINR_est);
	}
	else{
		CalSymb_mmse(chest_calsym_args.CFR_est, chest_calsym_args.SignalRec, chest_calsym_args.RxAntNum, chest_calsym_args.CarrierNum_SB, chest_calsym_args.LayerNum, chest_calsym_args.SymbNum, sqrt(sigma2), chest_calsym_args.Step, chest_calsym_args.flg_ave, chest_calsym_args.SymbEst,chest_calsym_args.SymbVar, chest_calsym_args.SINR_est, chest_calsym_args.CalSymb_p);
	}
	
	//Demod Mapping LLRD
	QAM_Demod_lut_2(chest_calsym_args.LLRD_Package, chest_calsym_args.SymbEst, chest_calsym_args.SymbVar, chest_calsym_args.SymbNum, chest_calsym_args.SymbolBitN_L, chest_calsym_args.LayerNum, chest_calsym_args.SBindex, chest_calsym_args.CarrierNum, chest_calsym_args.CarrierNum_SB, chest_calsym_args.PilotSymbNum);
	
	pthread_mutex_lock(&mutex1_rx);
	chest_calsym_args.ServiceEN_rx[chest_calsym_args.ServiceEN_index]++;
	pthread_mutex_unlock(&mutex1_rx);
}
