#ifndef CALSYMB_
#define CALSYMB_
void CalPilotSymb(int LayerNum, int CarrierNum, int PilotSymb_N, int User_idx, int inter_freq, lapack_complex_float *PilotSymb);
void CalSymb_mmse(lapack_complex_float *h_tmp, lapack_complex_float *Signal, int RxAntNum, int CarrierNum, int LayerNum, int SymbNum, float sigma, int Step, int flg_ave, lapack_complex_float *SymbEst,float *SymbVar, float *SINR_est);
void CalSymb_mmse_1(lapack_complex_float *h_tmp, lapack_complex_float *Signal, int RxAntNum, int CarrierNum, int SymbNum, float sigma, int Step, int flg_ave, lapack_complex_float *SymbEst, float *SymbVar, float *SINR_est);
#endif
