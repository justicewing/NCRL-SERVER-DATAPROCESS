#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include "srslte/fec/crc.h"
#include "srslte/fec/cbsegm.h"
#include "crc_cbsegm.h"

extern pthread_mutex_t mutex1_tx;

void crc_cbsegm(void *arg){

	struct crc_cbsegm_args_t crc_cbsegm_args = *((struct crc_cbsegm_args_t *)arg);

	srslte_crc_attach_byte(crc_cbsegm_args.crc_p, crc_cbsegm_args.tb, crc_cbsegm_args.tbs);
	srslte_cbsegm(crc_cbsegm_args.cb_tx,crc_cbsegm_args.tbs);
	
	pthread_mutex_lock(&mutex1_tx);
	crc_cbsegm_args.ServiceEN[crc_cbsegm_args.ServiceEN_index]++;
	pthread_mutex_unlock(&mutex1_tx);

}
