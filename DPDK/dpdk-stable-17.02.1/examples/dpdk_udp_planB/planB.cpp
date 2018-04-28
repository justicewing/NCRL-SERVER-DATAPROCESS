#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <complex.h>
#include <windows.h> 

float	Map4QAM[2] = { -0.707106781186547, 0.707106781186547 };
float	Map16QAM[4] = { -0.31622776601684, -0.94868329805051,  0.31622776601684,  0.94868329805051 };
float	Map64QAM[8] = { -0.46291004988628, -0.15430334996209, -0.77151674981046, -1.08012344973464,
0.46291004988628,  0.15430334996209,  0.77151674981046,  1.08012344973464 };
float	Map256QAM[16] = { -0.38348249442369, -0.53687549219316, -0.23008949665421, -0.07669649888474,
-0.84366148773211, -0.69026848996263, -0.99705448550158, -1.15044748327106,
0.38348249442369,  0.53687549219316,  0.23008949665421,  0.07669649888474,
0.84366148773211,  0.69026848996263,  0.99705448550158,  1.15044748327106 };

void chanload(float *HRe, float *HIm) {

	FILE *fp_hr;
	fp_hr = fopen("H_real.txt", "r");
	int m = 0;
	if (!fp_hr) printf("Can not open the file.\n");
	while (!feof(fp_hr)) {
		fscanf(fp_hr, "%f", &(HRe[m]));
		//if(m < 10) printf("%d %e\n",m,channel[m].real); 
		m++;
	}
	fclose(fp_hr);
	//printf("m = %d\n",m);

	FILE *fp_hi;
	fp_hi = fopen("H_imag.txt", "r");
	m = 0;
	if (!fp_hi) printf("Can not open the file.\n");
	while (!feof(fp_hi)) {
		fscanf(fp_hi, "%f", &(HIm[m]));
		//if(m < 10) printf("%d %e\n",m,channel[m].imag);
		m++;
	}
	fclose(fp_hi);

}

void getchan(float *Magn, float *Phase, float *HRe, float *HIm, int num) {
	float a, b;
	for (int i = 0; i < 2048; ++i) {
		a = HRe[i + num * 2048];
		b = HIm[i + num * 2048];
		Magn[i] = sqrt(a*a+b*b);
		Phase[i] = atan(b/a);
	}
}

void gencons(float *consRe, float *consIm, int order, float SNR) {
	srand((unsigned)time(0));
	for(int i = 0; i < 2048; ++i) {
		consRe[i] = Map4QAM[rand() % order] + rand() / (double)(RAND_MAX) * SNR;
		consIm[i] = Map4QAM[rand() % order] + rand() / (double)(RAND_MAX)* SNR;
	}
}

int main() {
	float *consRe = (float *)malloc(sizeof(float) * 2048);
	float *consIm = (float *)malloc(sizeof(float) * 2048);
	gencons(consRe, consIm, 4, 0.01);
	for (int i = 0; i < 2048; ++i) {
		printf("%f+%fi ",consRe[i], consIm[i]);
	}

	float *HRe = (float *)malloc(sizeof(float) * 2048 * 1000);
	float *HIm = (float *)malloc(sizeof(float) * 2048 * 1000);
	chanload(HRe, HIm);
	
	float *Magn = (float *)malloc(sizeof(float) * 2048);
	float *Phase = (float *)malloc(sizeof(float) * 2048);
	getchan(Magn, Phase, HRe, HIm, 454);
	
	for (int i = 0; i < 2048; ++i) {
		printf("A : %f\tQ : %f\n", Magn[i], Phase[i]);
	}

	free(consRe);
	free(consIm);
	free(HRe);
	free(HIm);
	free(Magn);
	free(Phase);
	return 0;
}
