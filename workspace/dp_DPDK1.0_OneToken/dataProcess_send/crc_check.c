#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/time.h>
#include "crc_check.h"

extern pthread_mutex_t mutex3_rx;

void crc_check(void *arg){

	struct crc_check_args_t crc_check_args = *((struct crc_check_args_t *)arg);
	int n = crc_check_args.subframeIndex;
	int i = crc_check_args.layer;
	
	int ZeroNum = 0;
	crc_check_args.checksum[i] = srslte_crc_checksum_byte(crc_check_args.crc_p, crc_check_args.tb, crc_check_args.tbs + crc_check_args.crc_length);

	if(crc_check_args.checksum[i] == 0){
		for(int i = 0; i < crc_check_args.tbs; i++){
			if(crc_check_args.tb[i] == 0) ZeroNum++;
		}
		if(ZeroNum == crc_check_args.tbs) crc_check_args.checksum[i] = 1;
	}

	pthread_mutex_lock(&mutex3_rx);
	crc_check_args.BERsignal[n]++;
	pthread_mutex_unlock(&mutex3_rx);
	
}

void BER_init(struct BER_t *BER, int MaxBeam){
	BER->BlocksNum = 0;
	BER->BLERNum = 0;
	BER->BlocksNum_L = (uint32_t *) malloc(sizeof(uint32_t) * MaxBeam);
	BER->BLERNum_L = (uint32_t *) malloc(sizeof(uint32_t) * MaxBeam);
	for(int i = 0; i < MaxBeam; i++){
		BER->BlocksNum_L[i] = 0;
		BER->BLERNum_L[i] = 0;
	}
	BER->BitsNum = 0;
	BER->BERNum = 0;
	BER->BitsNum_L = (uint32_t *) malloc(sizeof(uint32_t) * MaxBeam);
	BER->BERNum_L = (uint32_t *) malloc(sizeof(uint32_t) * MaxBeam);
	for(int i = 0; i < MaxBeam; i++){
		BER->BitsNum_L[i] = 0;
		BER->BERNum_L[i] = 0;
	}
}

