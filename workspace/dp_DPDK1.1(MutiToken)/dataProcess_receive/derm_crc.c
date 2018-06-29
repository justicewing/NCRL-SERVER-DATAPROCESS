#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <complex.h>
#include "mkl.h"
#include "derm_crc.h"
#include "srslte/fec/crc.h"
#include "srslte/fec/turbocoder.h"
#include "srslte/fec/rm_turbo.h"
#include "srslte/utils/vector.h"

extern pthread_mutex_t mutex2_rx;

void derm_crc(void *arg)
{

	struct derm_crc_args_t derm_crc_args = *((struct derm_crc_args_t *)arg);
	int s = derm_crc_args.symindex;
	//derm
	bzero(derm_crc_args.cb_tdec, 6176 * 3 * sizeof(int16_t));

	// FILE *fp = fopen("derm.txt", "a");
	// for (int i = 0; i < 32; i++)
	// 	fprintf(fp, "tdec0[%d]:%d\n", i, derm_crc_args.cb_tdec[i]);

	srslte_rm_turbo_rx_lut(derm_crc_args.LLRD_Package + s * derm_crc_args.SymbolBitN, derm_crc_args.cb_tdec, derm_crc_args.cbs_rm, derm_crc_args.detc_cb_sizes[derm_crc_args.Kr], derm_crc_args.rv_idx);

	// for (int i = 0; i < 32; i++)
	// 	fprintf(fp, "tdec[%d]:%d\n", i, derm_crc_args.cb_tdec[i]);
	// for (int i = 0; i < 32; i++)
	// 	fprintf(fp, "LLRD_Package[%d]:%d\n", i, derm_crc_args.LLRD_Package[i]);
	// fprintf(fp, "symindex:%d\n", derm_crc_args.symindex);
	// fprintf(fp, "SymbolBitN:%d\n", derm_crc_args.SymbolBitN);
	// fprintf(fp, "cbs_rm:%d\n", derm_crc_args.cbs_rm);
	// fprintf(fp, "detc_cb_sizes[%d]:%d\n", derm_crc_args.Kr, derm_crc_args.detc_cb_sizes[derm_crc_args.Kr]);
	// fprintf(fp, "rv_idx:%d\n", derm_crc_args.rv_idx);
	// fprintf(fp, "\n");
	// fclose(fp);

	for (int i = 0; i < 3 * derm_crc_args.Kr + 12; i++)
	{
		if (derm_crc_args.cb_tdec[i] > 64)
			derm_crc_args.cb_tdec[i] = 64;
		else if (derm_crc_args.cb_tdec[i] < -64)
			derm_crc_args.cb_tdec[i] = -64;
	}

	//decode
	srslte_tdec_run_all(derm_crc_args.tdec, derm_crc_args.cb_tdec, derm_crc_args.cb_crc, derm_crc_args.nof_iterations, derm_crc_args.Kr);

	//crc
	int c = derm_crc_args.conindex / 8;
	if (derm_crc_args.cbnum > 1)
	{
		derm_crc_args.checksum[derm_crc_args.r] = srslte_crc_checksum_byte(derm_crc_args.crc_p, derm_crc_args.cb_crc, derm_crc_args.Kr);
		for (int i = (derm_crc_args.r == 0) ? derm_crc_args.F / 8 : 0; i < (derm_crc_args.Kr - derm_crc_args.crc_length) / 8; i++)
		{
			derm_crc_args.tb[c] = derm_crc_args.cb_crc[i];
			c++;
		}
	}
	else
	{
		for (int i = derm_crc_args.F / 8; i < derm_crc_args.Kr / 8; i++)
		{
			derm_crc_args.tb[c] = derm_crc_args.cb_crc[i];
			c++;
		}
	}

	pthread_mutex_lock(&mutex2_rx);
	derm_crc_args.ServiceEN_rx[derm_crc_args.ServiceEN_index]++;
	pthread_mutex_unlock(&mutex2_rx);
}
