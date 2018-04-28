#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mkl.h"


int Chu[4] = { 11, 13, 23, 37 };
float PI = 3.141592653589793;


void fft(lapack_complex_float *vec, int len, lapack_complex_float *res);

//每个用户的层数不同
//输出：PilotSymb （LayerNum，CarrierNum,PilotSymb_N）

void CalPilotSymb(int LayerNum, int CarrierNum, int PilotSymb_N, int User_idx, int inter_freq, lapack_complex_float *PilotSymb) {
	int i, j, c;
	int k = CarrierNum / inter_freq;
	int ChuNum = *(Chu + User_idx - 1);
	lapack_complex_float *a;
	lapack_complex_float *w;
	lapack_complex_float *PilotSymb_temp;
	lapack_complex_float *PilotSymb_fft;

	a = (lapack_complex_float*)malloc(sizeof(lapack_complex_float) * k);
	w = (lapack_complex_float*)malloc(sizeof(lapack_complex_float) * CarrierNum);
	PilotSymb_temp = (lapack_complex_float*)malloc(sizeof(lapack_complex_float) * CarrierNum);
	PilotSymb_fft = (lapack_complex_float*)malloc(sizeof(lapack_complex_float) * CarrierNum);

	//每个符号的导频符号是相同的
	for (i = 0; i < LayerNum; ++i) {
		//ZC序列
		for (j = 0; j < k; ++j) {
			a[j].real = cos(PI * ChuNum * j * j / k);
			a[j].imag = sin(PI * ChuNum * j * j / k);
		}
		//print_clx(1, k, "a", a);

		for (j = 0; j < CarrierNum; ++j) {
			w[j].real = cos(2 * PI * i * j / CarrierNum);
			w[j].imag = sin(2 * PI * i * j / CarrierNum);
		}
		//print_clx(1, CarrierNum, "w", w);
		for (j = 0; j < CarrierNum; ++j) {
			//PilotSymb_temp[j].real = a[j % k].real;
			//PilotSymb_temp[j].imag = a[j % k].imag;

			PilotSymb_temp[j].real = (a[j % k].real * w[j].real - a[j % k].imag * w[j].imag) / sqrt(CarrierNum);
			PilotSymb_temp[j].imag = (a[j % k].real * w[j].imag + a[j % k].imag * w[j].real) / sqrt(CarrierNum);
		}
		//print_clx(1, CarrierNum, "PilotSymb_temp", PilotSymb_temp);
		fft(PilotSymb_temp, CarrierNum, PilotSymb_fft);
		//print_clx(1, CarrierNum, "PilotSymb_fft", PilotSymb_fft);

		//PilotSymb  (LayerNum,CarrierNum,PilotSymb_N)
		for (j = 0; j < CarrierNum; ++j) {
			for (c = 0; c < PilotSymb_N; ++c) {
				PilotSymb[c * CarrierNum * LayerNum + j * LayerNum + i].real = PilotSymb_fft[j].real;
				PilotSymb[c * CarrierNum * LayerNum + j * LayerNum + i].imag = PilotSymb_fft[j].imag;
			}
		}

	}
	free(a);
	free(w);
	free(PilotSymb_temp);
	free(PilotSymb_fft);
}


void fft(lapack_complex_float *vec, int len, lapack_complex_float *res) {
	int i, j;

	for (i = 0; i < len; ++i) {
		res[i].real = 0;
		res[i].imag = 0;
		for (j = 0; j < len; ++j) {
			res[i].real = res[i].real + vec[j].real * cos(2 * PI * i * j / len) + vec[j].imag * sin(2 * PI * i * j / len);
			res[i].imag = res[i].imag + vec[j].imag * cos(2 * PI * i * j / len) - vec[j].real * sin(2 * PI * i * j / len);
		}
	}

}

