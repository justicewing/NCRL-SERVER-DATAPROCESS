#ifndef REST_
#define REST_
struct Rest_t{
	int *PilotFreqIndex;
	lapack_complex_float *SignalRecPilot;
	lapack_complex_float *Pilot_nTx;
	lapack_complex_float *CFR_est_Pilot;
	lapack_complex_float *h_dct;
	float *R_tmp;
};
void REstimator_init(struct Rest_t *Rest_p, int CarrierNum, int inter_freq, int SymbNum);
float REstimator(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int SymbNum, int PilotSymbNum, int inter_freq, float *R_est, struct Rest_t *Rest_p);
void REstimator_free(struct Rest_t *Rest_p);
#endif
