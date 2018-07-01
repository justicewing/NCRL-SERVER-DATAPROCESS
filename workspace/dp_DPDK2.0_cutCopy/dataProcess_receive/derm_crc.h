#ifndef DEMCRC_
#define DEMCRC_
#include "srslte/fec/turbodecoder.h"
#include "srslte/fec/crc.h"

void derm_crc(void *arg);

struct derm_crc_args_t{
	int Kr;
	int r;
	int cbnum;
	//demod
	int16_t *LLRD_Package;
	int SymbolBitN;
	int modSymNum;
	int symindex;
	int LayerNum;
	
	//derm
	int cbs_rm;
	uint32_t rv_idx;
	int *detc_cb_sizes;
	
	//decode
	int16_t *cb_tdec;
	int cbs_tdec;
	srslte_tdec_t *tdec;
	uint32_t nof_iterations;
	
	//crc
	uint8_t *cb_crc;
	int cbs;
	srslte_crc_t *crc_p;
	uint32_t crc_poly;
	int crc_length;
	uint32_t *checksum;
	
	//cbcon
	uint8_t *tb;
	int conindex;
	int F;
	
	int ServiceEN_index;
	int *ServiceEN_rx;
};

#endif
