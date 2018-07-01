 #ifndef CRCTCODRM_
#define CRCTCODRM_

void crc_mod(void *arg);

struct crc_mod_args_t{
	int cbindex;
	srslte_cbsegm_t *cb_tx;
	uint8_t *tbp;
	
	//crc
	srslte_crc_t *crc_p;
	uint32_t crc_poly;
	int crc_length;

	uint8_t *cb;	
	//turbo edcode
	srslte_tcod_t *tcod;
	uint8_t *cb_tcod;
	
	//rate match
	int cbs_rm;
	uint8_t *cb_rm;
	uint8_t *cb_rm_bytes;
	uint8_t *w_buff;
	uint32_t rv_idx;
	uint8_t *parity_bytes;
	int *detc_cb_sizes;
	uint32_t max_rm_data_len_tx;
	
	//mod
	int SymbolBitN;
	int modSymNum;
	
	lapack_complex_float *package;
	//cb concatenation
	uint8_t *tb;
	
	int *ServiceEN;
	int ServiceEN_index;

};
#endif
