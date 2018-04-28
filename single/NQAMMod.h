#ifndef MOD_
#define MOD_
void QAM_Modulation(uint8_t *inbit, lapack_complex_float *Sig,int SymbolBitN, int SigLen);
void QAM_Demodulation(float* LLRD, lapack_complex_float* Recv, float* Sigma2, float* LLRA, int SymbNum, int SymbolBitN, int TxAntNum);
void genQAMtable();
int QAM_Demod_lut(float* LLRD, lapack_complex_float* Recv, float* Sigma2, float* LLRA, int SymbolNum, int SymbolBitN, int TxAntNum);
void freeQAMtable();
#endif
