#ifndef TBCRC_
#define TBCRC_
#include "srslte/fec/crc.h"
void crc_check(void *arg);

struct BER_t{
	uint32_t BlocksNum;//传输块数
	uint32_t BLERNum;////误块数
	uint32_t *BlocksNum_L;//各流传输块数
	uint32_t *BLERNum_L;//各流误块数
	uint32_t BitsNum;//传输比特数
	uint32_t BERNum;//误码数
	uint32_t *BitsNum_L;//各流传输比特数
	uint32_t *BERNum_L;//各流误码数
};

void BER_init(struct BER_t *BER, int MaxBeam);

struct crc_check_args_t{
	//crc
	srslte_crc_t *crc_p;
	uint32_t crc_poly;
	int crc_length;
	int tbs;
	int layer;
	uint8_t *tb;
	uint32_t *checksum;
	
	int subframeIndex;
	int *BERsignal;

};
#endif
