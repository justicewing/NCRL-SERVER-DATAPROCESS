#ifndef ADPTLINK_
#define ADPTLINK_
void RxAdptLink(lapack_complex_float *UserCh, float *SymbVarEst, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int SymbolNum, int *CQI_feedback);
void genAdptLinktable();
void RxAdptLink_lut(lapack_complex_float *UserCh, float *SymbVarEst, int TxAntNum, int RxAntNum, int CarrierNum, int UserNum, int SymbolNum, int *CQI_feedback);
void freeAdptLinktable();
#endif
