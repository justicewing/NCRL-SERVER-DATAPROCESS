#ifndef CRCCSB_
#define CRCCSB_
void crc_cbsegm(void *arg);

struct crc_cbsegm_args_t{
	//crc
	srslte_crc_t *crc_p;
	uint32_t crc_poly;
	int crc_length;
	int tbs;

	uint8_t *tb;
	//cbsegm
	srslte_cbsegm_t *cb_tx;
	
	int *ServiceEN;
	int ServiceEN_index;

};
#endif
