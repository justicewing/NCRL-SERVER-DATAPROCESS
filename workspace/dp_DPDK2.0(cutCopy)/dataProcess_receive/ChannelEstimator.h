#ifndef CHestDCT_
#define CHestDCT_
struct chestDCT_t{
	int *PilotFreqIndex;
	float *PG_i;
	float *PG_est;
	lapack_complex_float *SignalRecPilot;
	lapack_complex_float *Pilot_nTx;
	lapack_complex_float *CFR_est_Pilot;
	lapack_complex_float *h_dct;
	lapack_complex_float *CFR_est_nCh;
};
void chestDCT_init(struct chestDCT_t *chestDCT_p, int CarrierNum, int inter_freq, int SymbNum);
void ChannelEstimator_DCT(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, float *R_DCT, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int MaxBeam, int PilotSymbNum, int SymbNum, int inter_freq, float sigma, lapack_complex_float *CFR_est, struct chestDCT_t *chestDCT_p);
void chestDCT_free(struct chestDCT_t *chestDCT_p);
#endif
