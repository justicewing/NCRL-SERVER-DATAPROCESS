#ifndef CHEST_
#define CHEST_
void ChannelEstimator_DCT(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, float *R_DCT, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int MaxBeam, int PilotSymbNum, int SymbNum, int inter_freq, float sigma, lapack_complex_float *CFR_est);
float NoiseEstimator(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int PilotSymbNum, int inter_freq);
float ChannelEstimator_LS(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int PilotSymbNum, int SymbNum, int inter_freq, lapack_complex_float *CFR_est);
float REstimator(lapack_complex_float *SignalRec, lapack_complex_float *PilotSymb, int *PilotSymbIndex, int TxAntNum, int RxAntNum, int CarrierNum, int SymbNum, int PilotSymbNum, int inter_freq, float *R_est);
#endif
