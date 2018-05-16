#ifndef CHESTLS_
#define CHESTLS_
struct chestLS_t{
	int *PilotFreqIndex;
	lapack_complex_float *SignalRecPilot;
	lapack_complex_float *Pilot_nTx;
	lapack_complex_float *CFR_est_Pilot;
	lapack_complex_float *h_dct;
	lapack_complex_float *CFR_est_nCh;
	float *TrigoMx;
};
void chestLS_init(struct chestLS_t *chestLS_p, int CarrierNum, int inter_freq, int SymbNum);
float ChannelEstimator_LS(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int PilotSymbNum, int SymbNum, int inter_freq, lapack_complex_float *CFR_est, struct chestLS_t *chestLS_p);
void chestLS_free(struct chestLS_t *chestLS_p);
#endif
