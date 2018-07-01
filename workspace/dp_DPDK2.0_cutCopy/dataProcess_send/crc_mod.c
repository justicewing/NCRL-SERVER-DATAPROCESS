#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include "mkl.h"
#include "srslte/fec/crc.h"
#include "srslte/fec/turbocoder.h"
#include "srslte/fec/turbodecoder.h"
#include "srslte/fec/rm_turbo.h"
#include "crc_mod.h"
#include "srslte/utils/vector.h"
#include "srslte/config.h"
#include "srslte/utils/bit.h"
#include "NQAMMod.h"

extern pthread_mutex_t mutex2_tx;

void crc_mod(void *arg){

	struct crc_mod_args_t crc_mod_args = *((struct crc_mod_args_t *)arg);
	int k = 0, s = 0, Kr = 0;
	int r = crc_mod_args.cbindex;
	if(r < crc_mod_args.cb_tx->C2){
		Kr = crc_mod_args.cb_tx->K2;
		s = (Kr - crc_mod_args.crc_length) * r - crc_mod_args.cb_tx->F;
	}
	else{
		Kr = crc_mod_args.cb_tx->K1;
		s = (crc_mod_args.cb_tx->K2 - crc_mod_args.crc_length) * crc_mod_args.cb_tx->C2 + (Kr - crc_mod_args.crc_length) * (r - crc_mod_args.cb_tx->C2) - crc_mod_args.cb_tx->F;
	}
	s = s / 8;
	if (r == 0){
		s += crc_mod_args.cb_tx->F / 8;
		for(k = 0; k < crc_mod_args.cb_tx->F / 8; ++k){
			crc_mod_args.cb[k] = 0;//add null bit
		}
	}
	while (k < (Kr - crc_mod_args.crc_length) / 8){
		crc_mod_args.cb[k] = crc_mod_args.tbp[s];
		++s;
		++k;
	}
	//crc attach
	if (crc_mod_args.cb_tx->C > 1){
		srslte_crc_attach(crc_mod_args.crc_p, crc_mod_args.cb, Kr - crc_mod_args.crc_length);
	}
	else {
		while (k < Kr / 8){
			crc_mod_args.cb[k] = crc_mod_args.tbp[s];
			++s;
			++k;
		}
	}

	//turbo encode
	srslte_tcod_encode_lut(crc_mod_args.tcod, crc_mod_args.cb, crc_mod_args.parity_bytes, crc_mod_args.detc_cb_sizes[Kr]);
	//rate match
	bzero(crc_mod_args.w_buff, 6176 * 3 * sizeof(uint8_t));
	bzero(crc_mod_args.cb_rm_bytes, (crc_mod_args.max_rm_data_len_tx / 8 + 1) * sizeof(uint8_t));
	srslte_rm_turbo_tx_lut(crc_mod_args.w_buff, crc_mod_args.cb, crc_mod_args.parity_bytes, crc_mod_args.cb_rm_bytes, crc_mod_args.detc_cb_sizes[Kr], crc_mod_args.cbs_rm, 0, crc_mod_args.rv_idx);
	srslte_bit_unpack_vector(crc_mod_args.cb_rm_bytes, crc_mod_args.cb_rm, crc_mod_args.cbs_rm);
	//printf("\nlayer %d CB %d : %d rm bits:", i, r, crc_mod_args.cbs_rm);
	//for (int n = 0; n < 80; ++n) printf("%d",crc_mod_args.cb_rm[n]);

	//mod
	QAM_Modulation(crc_mod_args.cb_rm, crc_mod_args.package, crc_mod_args.SymbolBitN, crc_mod_args.modSymNum);
	//printf("\n%d modulation symblos in layer %d:", crc_mod_args.modSymNum, i);

	
	pthread_mutex_lock(&mutex2_tx);
	crc_mod_args.ServiceEN[crc_mod_args.ServiceEN_index]++;
	pthread_mutex_unlock(&mutex2_tx);
}
