#include "ldpc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int set_iLS[ILS_NUM] = { 2, 4, 8, 16, 32, 64, 128, 256,
							   3, 6, 12, 24, 48, 96, 192, 384,
							   5, 10, 20, 40, 80, 160, 320,
							   7, 14, 28, 56, 112, 224,
							   9, 18, 36, 72, 144, 288,
							   11, 22, 44, 88, 176, 352,
							   13, 26, 52, 104, 208,
							   15, 30, 60, 120, 240 };

static int all_iLS[ILS_NUM] = { 1, 1, 1, 1, 1, 1, 1, 1,
							   2, 2, 2, 2, 2, 2, 2, 2,
							   3, 3, 3, 3, 3, 3, 3,
							   4, 4, 4, 4, 4, 4,
							   5, 5, 5, 5, 5, 5,
							   6, 6, 6, 6, 6, 6,
							   7, 7, 7, 7, 7,
							   8, 8, 8, 8, 8 };

void nr15_fec_ldpc_param_init(nr15_ldpc_t *h, int B, double code_rate)
{
	int B1, L, K_p, K_n, K_b, Z_c, K_cb, C, K,N;
	int base_graph_mode, iLS, row_hbg, col_hbg;

	h->K_cb = 8448;
	K_cb = h->K_cb;

	if (B <= K_cb)
	{
		L = 0;
		C = 1;
		B1 = B;
	}
	else
	{
		L = 24;
		C = (B - 1) / (K_cb - L) + 1;
		B1 = B + C * L;
	}

	K_p = (B1 - 1) / C + 1;
	K_n = B1 / C;

	if (code_rate > 0.67)
	{
		base_graph_mode = 1;
		K_b = 22;
		Z_c = MAX_SET_ILS + 1;
		for (int i = 0; i < ILS_NUM; i++)
			if (set_iLS[i] * K_b - K_p >= 0 && set_iLS[i] < Z_c)
			{
				Z_c = set_iLS[i];
				iLS = all_iLS[i];
			}

		K = 22 * Z_c;
	}
	else
	{
		base_graph_mode = 2;
		if (B > 640)
			K_b = 10;
		else if (B > 560)
			K_b = 9;
		else if (B > 192)
			K_b = 8;
		else
			K_b = 6;
		Z_c = MAX_SET_ILS + 1;
		for (int i = 0; i < ILS_NUM; i++)
			if (set_iLS[i] * K_b - K_p >= 0 && set_iLS[i] < Z_c)
			{
				Z_c = set_iLS[i];
				iLS = all_iLS[i];
			}

		if (Z_c == MAX_SET_ILS + 1)
		{
			base_graph_mode = 1;
			K_b = 22;
			Z_c = MAX_SET_ILS + 1;
			for (int i = 0; i < ILS_NUM; i++)
				if (set_iLS[i] * K_b - K_p >= 0 && set_iLS[i] < Z_c)
				{
					Z_c = set_iLS[i];
					iLS = all_iLS[i];
				}
			K = 22 * Z_c;
		}
		else
			K = 10 * Z_c;
	}

	if (base_graph_mode == 1)
	{
		row_hbg = 46;
		col_hbg = 68;
		N = 66 * Z_c;
	}
	else
	{
		row_hbg = 42;
		col_hbg = 52;
		N = 50 * Z_c;
	}


	h->K = K;
	h->B = B;
	h->K_b = K_b;
	h->K_p = K_p;
	h->K_n = K_n;
	h->C = C;
	h->L = L;
	h->iLS = iLS;
	h->BG_sel = base_graph_mode;
	h->Z_c = Z_c;
	h->N = N;
	h->row_hbg = row_hbg;
	h->col_hbg = col_hbg;

	// ≥ı ºªØæÿ’Û
	int16_t** H_BG;
	H_BG = (int16_t **)malloc(sizeof(int16_t) * row_hbg);
	H_BG[0] = (int16_t *)malloc(sizeof(int16_t) * row_hbg * col_hbg);
	for (int i = 1; i < row_hbg; i++)
		H_BG[i] = H_BG[i - 1] + col_hbg;
	nr15_ldpc_matrix_init(h, h->H_BG);
		
}

void nr15_ldpc_matrix_init(nr15_ldpc_t *h, int16_t **H_BG)//, mod2sparse *H)
{
	int row_hbg = h->row_hbg;
	int col_hbg = h->col_hbg;
	int BG_sel = h->BG_sel;
	int iLS = h->iLS;

	char hbg_name[8] = "BG0_iLS0";
	hbg_name[2] = hbg_name[2] + (char)BG_sel;
	hbg_name[7] = hbg_name[7] + (char)iLS;
	FILE *fbg = fopen("BG1_iLS1", "a");
	for (int i = 0; i < row_hbg; i++)
		for (int j = 0; j < col_hbg; j++)
			fscanf(fbg, "%hd", &H_BG[i][j]);
	fclose(fbg);
}