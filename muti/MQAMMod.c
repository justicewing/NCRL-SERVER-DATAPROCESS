#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "mkl.h"
#include "srslte/config.h"


void QAM_Modulation(uint8_t *inbit, 
		    lapack_complex_float *Sig,
		    int	SymbolBitN, 
		    int	SigLen)
{

/* Linear Modulation */
/*
float	Map4QAM[2] = {-0.707106781186547, 0.707106781186547};
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
	int	M;
	int	i,j;
	int	Tmp_I, Tmp_Q;
	
	M = SymbolBitN/2;
	
	for(i=0; i<SigLen; i++)
	{
		Tmp_I = 0;
		Tmp_Q = 0;
		
		for(j=0; j<M; j++)
		{
			Tmp_I = Tmp_I + (inbit[i*SymbolBitN+j]<<(M-j-1));
			Tmp_Q = Tmp_Q + (inbit[i*SymbolBitN+M+j]<<(M-j-1));
		}
		switch (SymbolBitN)
		{
			case 2:
				Sig[i].real = Map4QAM[Tmp_I];
				Sig[i].imag = Map4QAM[Tmp_Q];
				break;
			case 4:
				Sig[i].real = Map16QAM[Tmp_I];
				Sig[i].imag = Map16QAM[Tmp_Q];
				break;
			case 6:
				Sig[i].real = Map64QAM[Tmp_I];
				Sig[i].imag = Map64QAM[Tmp_Q];
				break;
			case 8:
				Sig[i].real = Map256QAM[Tmp_I];
				Sig[i].imag = Map256QAM[Tmp_Q];
				break;
				
			default: printf("Invalid SymbolitN");
		}
	}
	
}

