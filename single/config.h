#ifndef CONFIG_S
#define CONFIG_S

#include <stdint.h>

const int crc_length = 24;
const uint32_t crc_24A = 0x1864CFB;
const uint32_t crc_24B = 0x1800063;
const int TOTALTAIL = 12;
const uint32_t BUFFSZ = 6176 * 3;

const int CarrierNum = 1200;
const int symbolNum = 14;
const int LayerNum = 128;
const int MaxBeam = 8;
const int RxAntNum = 8;

const int CQI_mod[16] = {0, 2, 2, 2, 2, 2,
						 2, 4, 4, 4, 6,
						 6, 6, 6, 6, 6};
const int CQI_cod[16] = {0, 78, 120, 193, 308, 449,
						 602, 378, 490, 616, 466,
						 567, 666, 772, 873, 948};
float CQI_mapping_table_LS[15] = {3, 4, 5, 7, 8,
								  9, 13, 14, 16, 17,
								  19, 20, 22, 24, 27};
float CQI_mapping_table_DCT[15] = {-4, -2, -1, 1, 3,
								   5, 8, 9, 11, 14,
								   16, 17, 19, 21, 24};

struct RunTime
{
	float crc;
	float cbsegm;
	float tcod;
	float rm;
	float mod;
	float pack;
	float others_tx;

	float noiseest;
	float chest;
	float sigdect;
	float cqi;
	float demod;
	float derm;
	float tdec;
	float crccheck;
	float others_rx;

	float tx;
	float rx;
};
#endif
