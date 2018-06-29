#ifndef SIGDET_
#define SIGDET_
struct CalSymb_t{
	lapack_complex_float *x_est;
	float *x_var;
	lapack_complex_float *ch_mx;
	lapack_complex_float *Signal_Rec;
	lapack_complex_float *ch_res;
	
	lapack_complex_float *ch_q;
	lapack_complex_float *ch_q1;
	lapack_complex_float *ch_q2;
	lapack_complex_float *Q_Matrix;
	float *lamda;
	lapack_complex_float *tau;
};
void CalSymb_mmse_1(lapack_complex_float *h_tmp, lapack_complex_float *Signal, int RxAntNum, int CarrierNum, int SymbNum, float sigma, int Step, int flg_ave, lapack_complex_float *SymbEst, float *SymbVar, float *SINR_est);
void CalSymb_init(struct CalSymb_t *CalSymb_p, int LayerNum, int RxAntNum);
void CalSymb_mmse(lapack_complex_float *h_tmp, lapack_complex_float *Signal, int RxAntNum, int CarrierNum, int LayerNum, int SymbNum, float sigma, int Step, int flg_ave, lapack_complex_float *SymbEst,float *SymbVar, float *SINR_est, struct CalSymb_t *CalSymb_p);
void CalSymb_free(struct CalSymb_t *CalSymb_p);
#endif
