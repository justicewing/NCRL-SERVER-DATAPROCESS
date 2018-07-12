#ifndef LDPC_H
#define LDPC_H

#include <stdint.h>
//#include "mod2sparse.h"

#define ILS_NUM 51
#define MAX_SET_ILS 384

typedef struct
{
	int B;
	int K;
	int N;
	int K_cb;
	int K_b;
	int K_p;
	int K_n;
	int C;
	int L;
	int iLS;
	int BG_sel;
	int Z_c;
	int row_hbg;
	int col_hbg;
	int16_t **H_BG;
} nr15_ldpc_t;

void nr15_fec_ldpc_param_init(nr15_ldpc_t *h, int B, double code_rate);

void nr15_ldpc_matrix_init(nr15_ldpc_t *h, int16_t **H_BG);// , mod2sparse *H);

#endif