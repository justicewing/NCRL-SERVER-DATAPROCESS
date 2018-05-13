#ifndef COMMON_
#define COMMON_

#include <mkl.h>
#include <stdint.h>

const int crc_length = 24;
const uint32_t crc_24A = 0x1864CFB;
const uint32_t crc_24B = 0x1800063;

const int TOTALTAIL = 12;
const uint32_t BUFFSZ = 6176 * 3;

const int CarrierNum = 1200;

const int TxAntNum = 128;
const int RxAntNum = 8;
const int MaxBeam = 8;
const int UserNum = 1;
const int PathNum = 6;
const int DataSymNum = 12;
const int PilotSymbNum = 2;
const int SymbolNum = 14;

const int CQI_mod[16] = {0, 2, 2, 2, 2, 2, 2, 4, 4, 4, 6, 6, 6, 6, 6, 6};
const int CQI_cod[16] = {0, 78, 120, 193, 308, 449, 602, 378, 490, 616, 466, 567, 666, 772, 873, 948};
const float CQI_mapping_table_LS[15] = {3, 4, 5, 7, 8, 9, 13, 14, 16, 17, 19, 20, 22, 24, 27};
const float CQI_mapping_table_DCT[15] = {-4, -2, -1, 1, 3, 5, 8, 9, 11, 14, 16, 17, 19, 21, 24};

const int taskNum_tx = 3;
const int taskNum_rx = 3;

const int datasymbNum[10] = {12408, 10728, 12540, 13200, 13200, 13128, 13128, 13200, 13200, 13200};
//  0 : D  12408 = 1128 * 11
//  1 : S  10728 = 1200 * 8 + 1128 * 1
//  2 : U  12540 = 1140 * 11
//  3 : U  13200 = 1200 * 11
//  4 : U  13200 = 1200 * 11
//  5 : D  13128 = 1200 * 10 + 1128 * 1
//  6 : D  13128 = 1200 * 10 + 1128 * 1
//  7 : D  13200 = 1200 * 11
//  8 : D  13200 = 1200 * 11
//  9 : D  13200 = 1200 * 11

struct package_t
{
	uint8_t *data[8];
	lapack_complex_float *y;
	int *tbs;
	int *CQI_index;
	float SNR;
};

struct test_rx_t
{
	uint8_t *data[8];
	float SNR;
	int packIdx;
};

struct RunTime
{
	float crc;
	float cbsegm;
	float task1_tx;

	float tcod;
	float rm;
	float mod;
	float task2_tx;

	float noiseest;
	float chest;
	float sigdect;
	float task1_rx;

	float demod;
	float derm;
	float tdec;
	float task2_rx;

	float crccheck;
	float task3_rx;

	float tx;
	float others_tx;
	float rx;
};
#endif
