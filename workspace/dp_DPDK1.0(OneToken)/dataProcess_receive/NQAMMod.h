#ifndef MOD_
#define MOD_
void QAM_Modulation(uint8_t *inbit, lapack_complex_float *Sig,int SymbolBitN, int SigLen);
void QAM_Demodulation(float* LLRD, lapack_complex_float* Recv, float* Sigma2, float* LLRA, int SymbNum, int SymbolBitN, int TxAntNum);
void genQAMtable();
int QAM_Demod_lut(int16_t* LLRD, lapack_complex_float* Recv, float* Sigma2, int SymbolNum, int SymbolBitN, int TxAntNum);
int QAM_Demod_lut_2(int16_t* LLRD, lapack_complex_float* Recv, float* Sigma2, int SymbolNum, int *SymbolBitN_L, int LayerNum, int SBindex,int CarrierNum,int CarrierNum_SB,int PilotSymbNum);
void freeQAMtable();
#endif
