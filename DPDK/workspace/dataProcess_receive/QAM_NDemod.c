#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mkl.h"

float *Sigma2Table;
float *Table4QAM;
float *Table16QAM;
float *Table64QAM;
int SymbOrder = 12;
int SigmaOrder = 4;
const float symbol_max[3] = {0.707106781186547*2,0.94868329805051*2,1.08012344973464*2};

float maxo(float a, float b)
{
	return (a>b)? a:b;
	//return (a>b)? a+log(1+exp(-fabs(a-b))):b+log(1+exp(-fabs(a-b)));
}

//输入参数：
//LLRD	     输出的软解调结果,编码比特序列的后验信息
//Recv       输入的信号检测的结果，信号的估计值
//Sigma2     信号的估计方差，是数组
//LLRA       编码比特序列的先验信息
//SymbNum    符号数目
//SymbolBitN 每个符号对应的二进制码字数目
//TxAntNum	 发送天线的数目

void QAM_Demodulation(float* LLRD, lapack_complex_float* Recv, float* Sigma2, float* LLRA, int SymbNum, int SymbolBitN, int TxAntNum)
{

/* Linear Modulation */
/*
float	Map16QAM[4] = {-0.94868329805051, -0.31622776601684,  0.31622776601684,  0.94868329805051};
float	Map64QAM[8] = {-1.08012344973464, -0.77151674981046, -0.46291004988628, -0.15430334996209,
		        0.15430334996209,  0.46291004988628,  0.77151674981046,  1.08012344973464};
float	Map256QAM[16] = {-1.15044748327106, -0.99705448550158, -0.84366148773211, -0.69026848996263,
			 -0.53687549219316, -0.38348249442369, -0.23008949665421, -0.07669649888474,
			  0.07669649888474,  0.23008949665421,  0.38348249442369,  0.53687549219316,
			  0.69026848996263,  0.84366148773211,  0.99705448550158,  1.15044748327106};
//*/

/* Gray Modulation */
//*
float	Map4QAM[2] = {-0.707106781186547, 0.707106781186547};
float	Map16QAM[4] = {-0.31622776601684, -0.94868329805051,  0.31622776601684,  0.94868329805051};
float	Map64QAM[8] = {-0.46291004988628, -0.15430334996209, -0.77151674981046, -1.08012344973464,
		        0.46291004988628,  0.15430334996209,  0.77151674981046,  1.08012344973464};
float	Map256QAM[16] = {-0.38348249442369, -0.53687549219316, -0.23008949665421, -0.07669649888474,
			 -0.84366148773211, -0.69026848996263, -0.99705448550158, -1.15044748327106,
			  0.38348249442369,  0.53687549219316,  0.23008949665421,  0.07669649888474,
			  0.84366148773211,  0.69026848996263,  0.99705448550158,  1.15044748327106};
//*/
	int	n_Tx, n_Sig, n_bit, n_const;
	int	M, ConstNum;
	
	float	nom[4] = {0,0,0,0}, denom[4] = {0,0,0,0}, inf;
	
	float	oldLLR[4];
	float	metric0, metric1, metric;
	
	float	Xi, Yi, vari;
	
	M = SymbolBitN/2;
	ConstNum = (1<<M);
	inf = 1e6;
	
	switch (SymbolBitN)
	{
		case 2:
			for(n_Sig=0; n_Sig<SymbNum; n_Sig++)
			{
				for(n_Tx=0; n_Tx<TxAntNum; n_Tx++)
				{
					Xi = Recv[n_Sig*TxAntNum+n_Tx].real;
					Yi = Recv[n_Sig*TxAntNum+n_Tx].imag;
					vari = Sigma2[n_Sig*TxAntNum+n_Tx];
					
					LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+0] = 4*Xi/Map4QAM[1]/vari;
					LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+1] = 4*Yi/Map4QAM[1]/vari;
					
				}
			}
			break;
		case 4:
			for(n_Sig=0; n_Sig<SymbNum; n_Sig++)
			{
				for(n_Tx=0; n_Tx<TxAntNum; n_Tx++)
				{
					Xi = Recv[n_Sig*TxAntNum+n_Tx].real;
					Yi = Recv[n_Sig*TxAntNum+n_Tx].imag;
					vari = Sigma2[n_Sig*TxAntNum+n_Tx];
					
					/* In-pase Component*/
					for(n_bit=0; n_bit<M; n_bit++)
					{
						nom[n_bit] = -inf;
						denom[n_bit] = -inf;
						oldLLR[n_bit] = LLRA[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit];
					}
					
					for(n_const=0; n_const<ConstNum; n_const++)
					{
						metric0 = 0;
						metric1 = 0;
						
						metric0 = -(Xi-Map16QAM[n_const])*(Xi-Map16QAM[n_const])/vari;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							metric1 = metric1 + (2*((n_const>>(M-1-n_bit))&1)-1)*oldLLR[n_bit];
						}
						
						metric = metric0 + 0.5*metric1;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							if ((n_const>>(M-1-n_bit))&1)
							{
								nom[n_bit] = maxo(nom[n_bit], metric);
							}
							else
							{
								denom[n_bit] = maxo(denom[n_bit], metric);
							}
						}
					}
					
					for(n_bit=0; n_bit<M; n_bit++)
					{
						LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] = nom[n_bit] - denom[n_bit] - oldLLR[n_bit];
					}
					
					/* Quadure Component*/
					for(n_bit=0; n_bit<M; n_bit++)
					{
						nom[n_bit] = -inf;
						denom[n_bit] = -inf;
						oldLLR[n_bit] = LLRA[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit+M];
					}
					
					for(n_const=0; n_const<ConstNum; n_const++)
					{
						metric0 = 0;
						metric1 = 0;
						
						metric0 = -(Yi-Map16QAM[n_const])*(Yi-Map16QAM[n_const])/vari;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							metric1 = metric1 + (2*((n_const>>(M-1-n_bit))&1)-1)*oldLLR[n_bit];
						}
						
						metric = metric0 + 0.5*metric1;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							if ((n_const>>(M-1-n_bit))&1)
							{
								nom[n_bit] = maxo(nom[n_bit], metric);
							}
							else
							{
								denom[n_bit] = maxo(denom[n_bit], metric);
							}
						}
					}
					
					for(n_bit=0; n_bit<M; n_bit++)
					{
						LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit+M] = nom[n_bit] - denom[n_bit] - oldLLR[n_bit];
					}
					
				}
			}
			break;
		case 6:
			for(n_Sig=0; n_Sig<SymbNum; n_Sig++)
			{
				for(n_Tx=0; n_Tx<TxAntNum; n_Tx++)
				{
					Xi = Recv[n_Sig*TxAntNum+n_Tx].real;
					Yi = Recv[n_Sig*TxAntNum+n_Tx].imag;
					vari = Sigma2[n_Sig*TxAntNum+n_Tx];
					
					/* In-pase Component*/
					for(n_bit=0; n_bit<M; n_bit++)
					{
						nom[n_bit] = -inf;
						denom[n_bit] = -inf;
						oldLLR[n_bit] = LLRA[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit];
					}
					
					for(n_const=0; n_const<ConstNum; n_const++)
					{
						metric0 = 0;
						metric1 = 0;
						
						metric0 = -(Xi-Map64QAM[n_const])*(Xi-Map64QAM[n_const])/vari;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							metric1 = metric1 + (2*((n_const>>(M-1-n_bit))&1)-1)*oldLLR[n_bit];
						}
						
						metric = metric0 + 0.5*metric1;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							if ((n_const>>(M-1-n_bit))&1)
							{
								nom[n_bit] = maxo(nom[n_bit], metric);
							}
							else
							{
								denom[n_bit] = maxo(denom[n_bit], metric);
							}
						}
					}
					
					for(n_bit=0; n_bit<M; n_bit++)
					{
						LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] = nom[n_bit] - denom[n_bit] - oldLLR[n_bit];
					}
					
					/* Quadure Component*/
					for(n_bit=0; n_bit<M; n_bit++)
					{
						nom[n_bit] = -inf;
						denom[n_bit] = -inf;
						oldLLR[n_bit] = LLRA[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit+M];
					}
					
					for(n_const=0; n_const<ConstNum; n_const++)
					{
						metric0 = 0;
						metric1 = 0;
						
						metric0 = -(Yi-Map64QAM[n_const])*(Yi-Map64QAM[n_const])/vari;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							metric1 = metric1 + (2*((n_const>>(M-1-n_bit))&1)-1)*oldLLR[n_bit];
						}
						
						metric = metric0 + 0.5*metric1;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							if ((n_const>>(M-1-n_bit))&1)
							{
								nom[n_bit] = maxo(nom[n_bit], metric);
							}
							else
							{
								denom[n_bit] = maxo(denom[n_bit], metric);
							}
						}
					}
					
					for(n_bit=0; n_bit<M; n_bit++)
					{
						LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit+M] = nom[n_bit] - denom[n_bit] - oldLLR[n_bit];
					}
					
				}
			}
			break;
		case 8:
			for(n_Sig=0; n_Sig<SymbNum; n_Sig++)
			{
				for(n_Tx=0; n_Tx<TxAntNum; n_Tx++)
				{
					Xi = Recv[n_Sig*TxAntNum+n_Tx].real;
					Yi = Recv[n_Sig*TxAntNum+n_Tx].imag;
					vari = Sigma2[n_Sig*TxAntNum+n_Tx];
					
					/* In-pase Component*/
					for(n_bit=0; n_bit<M; n_bit++)
					{
						nom[n_bit] = -inf;
						denom[n_bit] = -inf;
						oldLLR[n_bit] = LLRA[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit];
					}
					
					for(n_const=0; n_const<ConstNum; n_const++)
					{
						metric0 = 0;
						metric1 = 0;
						
						metric0 = -(Xi-Map256QAM[n_const])*(Xi-Map256QAM[n_const])/vari;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							metric1 = metric1 + (2*((n_const>>(M-1-n_bit))&1)-1)*oldLLR[n_bit];
						}
						
						metric = metric0 + 0.5*metric1;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							if ((n_const>>(M-1-n_bit))&1)
							{
								nom[n_bit] = maxo(nom[n_bit], metric);
							}
							else
							{
								denom[n_bit] = maxo(denom[n_bit], metric);
							}
						}
					}
					
					for(n_bit=0; n_bit<M; n_bit++)
					{
						LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] = nom[n_bit] - denom[n_bit] - oldLLR[n_bit];
					}
					
					/* Quadure Component*/
					for(n_bit=0; n_bit<M; n_bit++)
					{
						nom[n_bit] = -inf;
						denom[n_bit] = -inf;
						oldLLR[n_bit] = LLRA[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit+M];
					}
					
					for(n_const=0; n_const<ConstNum; n_const++)
					{
						metric0 = 0;
						metric1 = 0;
						
						metric0 = -(Yi-Map256QAM[n_const])*(Yi-Map256QAM[n_const])/vari;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							metric1 = metric1 + (2*((n_const>>(M-1-n_bit))&1)-1)*oldLLR[n_bit];
						}
						
						metric = metric0 + 0.5*metric1;
						
						for(n_bit=0; n_bit<M; n_bit++)
						{
							if ((n_const>>(M-1-n_bit))&1)
							{
								nom[n_bit] = maxo(nom[n_bit], metric);
							}
							else
							{
								denom[n_bit] = maxo(denom[n_bit], metric);
							}
						}
					}
					
					for(n_bit=0; n_bit<M; n_bit++)
					{
							LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit+M] = nom[n_bit] - denom[n_bit] - oldLLR[n_bit];
					}
					
				}
			}
			break;
		default: printf("Invalid SymbolBitN!\n");
	}
	for(n_Sig=0; n_Sig<SymbNum; n_Sig++)
	{
		for(n_Tx=0; n_Tx<TxAntNum; n_Tx++)
		{
			for(n_bit=0; n_bit<SymbolBitN; n_bit++)
			{
				LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] = (LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] > 64)? 64:LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit];
				LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] = (LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit] < -64)? -64:LLRD[n_Sig*TxAntNum*SymbolBitN+n_Tx*SymbolBitN+n_bit];
			}
		}
	}
}

void genQAMtable(){

	float  sigma_max= log10(1);
	float sigma_min = log10(0.0001);

	float step = 0;
	int SymbNum = pow(2,SymbOrder/2);
	int SigmaNum = pow(2,SigmaOrder);
	float *SymbolTable = (float *) malloc(sizeof(float) * SymbNum);
	Sigma2Table = (float *) malloc(sizeof(float) * SigmaNum);
	
	float *LLRA = (float *) calloc(6,sizeof(float));
	float *LLRD = (float *) calloc(6,sizeof(float));
	lapack_complex_float *Symbol = (lapack_complex_float *) calloc(1,sizeof(lapack_complex_float));
	float *Sigma2 = (float *) calloc(1,sizeof(float));
	
	int index = 0;
	Table4QAM = (float *) calloc(SymbNum * SymbNum * SigmaNum * 2,sizeof(float));
	Table16QAM = (float *) calloc(SymbNum * SymbNum * SigmaNum * 4,sizeof(float));
	Table64QAM = (float *) calloc(SymbNum * SymbNum * SigmaNum * 6,sizeof(float));

	float *Table;
	for(int n = 0; n < 3; n++){
		//printf("===== Map Point %d =====:\n",n);
		
		//printf("Sigma2Table : ");
		step = (sigma_max - sigma_min) / (SigmaNum - 1);
		for (int i = 0; i < SigmaNum; i++){
			Sigma2Table[i] = pow(10, sigma_min + step * i);
			//printf("%f ",Sigma2Table[i]);
		}
		
		//printf("\nsymbolTable :");
		step = symbol_max[n] * 2 / SymbNum;
		for (int i = 0; i < SymbNum / 2; i++){
			SymbolTable[i] = step * (i + 1);
			//printf("%f(%d) ",SymbolTable[i],i);
		}
		for (int i = SymbNum / 2; i < SymbNum; i++){
			SymbolTable[i] = -SymbolTable[i - SymbNum / 2];
			//printf("%f(%d) ",SymbolTable[i],i);
		}
		//printf("\n");
		
		switch (n){
		case 0 :
			Table = Table4QAM;
			break;
		case 1 :
			Table = Table16QAM;
			break;
		case 2 :
			Table = Table64QAM;
			break;
		}
		
		//printf("===== Table %d =====:\n",n+1);
		int SymbolBitN = 2 * (n + 1);
		for (int i = 0; i < SymbNum; i++){
			Symbol[0].real = SymbolTable[i];
			for (int j = 0; j < SymbNum; j++){
				Symbol[0].imag = SymbolTable[j];
				for (int k = 0; k < SigmaNum; k++){
					Sigma2[0] = Sigma2Table[k];
					//printf("Symbol : %f+%fi sigma2 : %f ",Symbol[0].real,Symbol[0].imag,Sigma2[0]);
					index = i * SymbNum * SigmaNum + j * SigmaNum + k;
					QAM_Demodulation(LLRD, Symbol, Sigma2, LLRA, 1, SymbolBitN, 1);
					//printf("LLRD : ");
					for(int m = 0; m < (n + 1) * 2; m++){
						Table[index * SymbolBitN + m] = LLRD[m];
						//printf("%f ",LLRD[m]);
					}
					//printf("\n");
				}
			}
		}
	}	
	
	free(SymbolTable);
	free(LLRA);
	free(LLRD);
	free(Symbol);
	free(Sigma2);
}

int QAM_Demod_lut(int16_t* LLRD, lapack_complex_float* Recv, float* Sigma2, int SymbolNum, int SymbolBitN, int TxAntNum){

	int SymbNum = pow(2,SymbOrder/2);
	int SigmaNum = pow(2,SigmaOrder);
	float step = 0;
	float *Table;
	switch (SymbolBitN){
		case 2 :
			Table = Table4QAM;
			break;
		case 4 :
			Table = Table16QAM;
			break;
		case 6 :
			Table = Table64QAM;
			break;
		default :
			printf("Invalid SymbolBitN");
			return -1;
	}
	
	int RsymbIndex = 0;
	int IsymbIndex = 0;
	int sigmaIndex = 0;
	int index = 0;
	int j = 0;
	step = symbol_max[SymbolBitN / 2 - 1] * 2 / SymbNum;
	float temp = 1 / step;
	//printf("\nstep:%f",step);
	for(int n = 0; n < SymbolNum; n++){
	
		//printf("\nsymbol : %f+%fi ",Recv[n].real,Recv[n].imag);
		//Real symbol
		if(Recv[n].real > 0){
			RsymbIndex = (int) (temp * Recv[n].real - 0.5);
			if(RsymbIndex >= SymbNum / 2) RsymbIndex = SymbNum / 2 - 1;
		}
		else{
			RsymbIndex = (int) (-temp * Recv[n].real - 0.5) + SymbNum / 2;
			if(RsymbIndex >= SymbNum) RsymbIndex = SymbNum - 1;
		}
		//printf("\nRsymbIndex:%d",RsymbIndex);
		//imag symbol
		if(Recv[n].imag > 0){
			IsymbIndex = (int) (temp * Recv[n].imag - 0.5);
			if(IsymbIndex >= SymbNum / 2) IsymbIndex = SymbNum / 2 - 1;
			
		}
		else{
			IsymbIndex = (int) (-temp * Recv[n].imag - 0.5) + SymbNum / 2;
			if(IsymbIndex >= SymbNum) IsymbIndex = SymbNum - 1;
		}
		//printf("\nIsymbIndex:%d",IsymbIndex);
		//sigma
		sigmaIndex = 0;
		while(Sigma2[n] > Sigma2Table[sigmaIndex]){
			sigmaIndex++;
			if(sigmaIndex == SigmaNum){
				sigmaIndex--;
				break;
			}
		}
		//printf("\nsigmaIndex:%d",sigmaIndex);
		
		index = RsymbIndex * SymbNum * SigmaNum + IsymbIndex * SigmaNum + sigmaIndex;
		for(int i = 0; i < SymbolBitN; i++){
			LLRD[j] = Table[index * SymbolBitN + i];
			j++;
		}
	}
	
	return 0;
}

int QAM_Demod_lut_2(int16_t* LLRD, lapack_complex_float* Recv, float* Sigma2, int SymbolNum, int *SymbolBitN_L, int LayerNum, int SBindex,int CarrierNum,int CarrierNum_SB,int PilotSymbNum){
	
	int SymbNum = pow(2,SymbOrder/2);
	int SigmaNum = pow(2,SigmaOrder);
	float step = 0,temp = 0;
	float *Table;
	
	int RsymbIndex = 0;
	int IsymbIndex = 0;
	int sigmaIndex = 0;
	int index = 0;
	int j = 0,k = 0;
	
	int m = SBindex;
	int SymbolBitN = 0;
	for(int i = 0; i < LayerNum; i++){
	
		SymbolBitN = SymbolBitN_L[i];
		step = symbol_max[SymbolBitN / 2 - 1] * 2 / SymbNum;
		temp = 1 / step;
		switch (SymbolBitN){
		case 2 :
			Table = Table4QAM;
			break;
		case 4 :
			Table = Table16QAM;
			break;
		case 6 :
			Table = Table64QAM;
			break;
		default :
			printf("Invalid SymbolBitN");
			return -1;
		}
		for(int ns = 0; ns < SymbolNum; ns++){
			if((ns != 3) && (ns != 10)){
				for(int nc = 0; nc < CarrierNum_SB; nc++){
					if(ns < 3){
						j = i * CarrierNum * (SymbolNum - PilotSymbNum) + ns * CarrierNum + nc + m * CarrierNum_SB;
					}
					else if(ns < 10){
						j = i * CarrierNum * (SymbolNum - PilotSymbNum) + (ns - 1) * CarrierNum + nc + m * CarrierNum_SB;
					}
					else{
						j = i * CarrierNum * (SymbolNum - PilotSymbNum) + (ns - 2) * CarrierNum + nc + m * CarrierNum_SB;
					}
					k = ns * CarrierNum_SB * LayerNum + nc * LayerNum + i;
					
					//Real symbol
					if(Recv[k].real > 0){
						RsymbIndex = (int) (temp * Recv[k].real - 0.5);
						if(RsymbIndex >= SymbNum / 2) RsymbIndex = SymbNum / 2 - 1;
					}
					else{
						RsymbIndex = (int) (-temp * Recv[k].real - 0.5) + SymbNum / 2;
						if(RsymbIndex >= SymbNum) RsymbIndex = SymbNum - 1;
					}
					//imag symbol
					if(Recv[k].imag > 0){
						IsymbIndex = (int) (temp * Recv[k].imag - 0.5);
						if(IsymbIndex >= SymbNum / 2) IsymbIndex = SymbNum / 2 - 1;
			
					}
					else{
						IsymbIndex = (int) (-temp * Recv[k].imag - 0.5) + SymbNum / 2;
						if(IsymbIndex >= SymbNum) IsymbIndex = SymbNum - 1;
					}
					//sigma
					sigmaIndex = 0;
					while(Sigma2[k] > Sigma2Table[sigmaIndex]){
						sigmaIndex++;
						if(sigmaIndex == SigmaNum){
							sigmaIndex--;
							break;
						}
					}
					//index
					index = RsymbIndex * SymbNum * SigmaNum + IsymbIndex * SigmaNum + sigmaIndex;
					for(int n = 0; n < SymbolBitN; n++){
						LLRD[j * SymbolBitN + n] = Table[index * SymbolBitN + n] > 0 ? Table[index * SymbolBitN + n] + 0.5 : Table[index * SymbolBitN + n] - 0.5;
					}
				}
			}
		}
	}
	return 0;
}

void freeQAMtable(){
	free(Sigma2Table);
	free(Table4QAM);
	free(Table16QAM);
	free(Table64QAM);
}

