#ifndef CHSYM_
#define CHSYM_
#include "ChannelEstimator_LS.h"
#include "ChannelEstimator.h"
#include "CalSymb_mmse.h"
#include "R_Estimator.h"
#include "CalPilotSymb.h"
#include "NQAMMod.h"
#include "RxAdptLink.h"
void chest_calsym(void *arg);

struct chest_calsym_args_t{
	lapack_complex_float *package;
	int SBNum;
	int SBindex;
	
	lapack_complex_float *SignalRec;
	lapack_complex_float *PilotSymb_SB;
	lapack_complex_float *PilotSymb;
	float *R_DCT;
	int *PilotSymbIndex;
	int TxAntNum;
	int RxAntNum;
	int CarrierNum;
	int CarrierNum_SB;
	int UserNum;
	int MaxBeam;
	int PilotSymbNum;
	int SymbNum;
	int inter_freq;
	int LayerNum;
	int Step;
	int flg_ave;
	int ChEstType;
	float *SINR_est;
	struct chestLS_t *chestLS_p;
	struct chestDCT_t *chestDCT_p;
	struct CalSymb_t *CalSymb_p;
	struct Rest_t *Rest_p;
	
	lapack_complex_float *CFR_est;
	lapack_complex_float *CFR;
	lapack_complex_float *CFR_SB;
	
	lapack_complex_float *SymbEst;
	float *SymbVar;
	
	int16_t *LLRD_Package;
	float *LLRA;
	int *SymbolBitN_L;
	
	int ServiceEN_index;
	int *ServiceEN_rx;
};
#endif
