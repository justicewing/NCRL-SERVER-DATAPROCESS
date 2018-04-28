#ifndef PACK_
#define PACK_

extern const int PackHeadLen = 8;
extern const int SubFrmHeadLen = 32;
extern const int SymbolHeadLen = 16;

void packing_S_B(void *arg);
void unpacking_B_S(void *arg);

struct packarg_t{
	
	uint32_t packIndex;

	uint32_t CarrierBegin;
	uint32_t CarrierEnd;
	
	//PDSCH PUSCH
	uint16_t *RealData;
	uint16_t *ImagData;
	//DM-RS
	uint16_t *RealPilo;
	uint16_t *ImagPilo;
	//PCFICH  PHICH  PDCCH
	uint16_t *RealPDCCH;
	uint16_t *ImagPDCCH;
	//PSS
	uint16_t *RealPSS;
	uint16_t *ImagPSS;
	//SSS
	uint16_t *RealSSS;
	uint16_t *ImagSSS;
	//SRS
	uint16_t *RealSRS;
	uint16_t *ImagSRS;
	//PBCH
	uint16_t *RealPBCH;
	uint16_t *ImagPBCH;
	//Channel calibration
	uint16_t *RealCalib;
	uint16_t *ImagCalib;
	//PRACH
	uint16_t *RealPRACH;
	uint16_t *ImagPRACH;
	
	uint32_t LayerNum;
	uint32_t CarrierNum;
	uint32_t TotalCarrierNum;
	uint32_t SymbNum;
	uint32_t FrmIdx;
	uint32_t SubFrmIdx;
	uint32_t SymbIdx;
	uint32_t *BfParSel;
	
	uint16_t *package;
};


#endif
