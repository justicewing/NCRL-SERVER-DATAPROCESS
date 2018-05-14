#ifndef MYCONFIG_H_M
#define MYCONFIG_H_M

#include <mkl.h>
#include <stdint.h>

#define CRC_LENGTH 24
#define CRC_24A 0x1864CFB
#define CRC_24B 0x1800063

#define TOTALTAIL 12
#define BUFFSZ 6176 * 3

#define CARRIER_NUM 1200
#define TX_ANT_NUM 128
#define RX_ANT_NUM 8
#define MAX_BEAM 8
#define USER_NUM 1
#define PATH_NUM 6
#define DATA_SYM_NUM 12
#define PILOT_SYM_NUM 2
#define SYMBOL_NUM (DATA_SYM_NUM + PILOT_SYM_NUM)
#define MAX_MOD 6

#define TASK_NUM_TX 3
#define TASK_NUM_RX 3

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

#define MAX_SYMBOL_NUM (CARRIER_NUM *DATA_SYM_NUM)
#define MAX_DATA_SIZE_TX (MAX_MOD / 1024.0 * MAX_SYMBOL_NUM *MAX_MOD - CRC_LENGTH)
#define SIZE_Y (MAX_BEAM *CARRIER_NUM *SYMBOL_NUM * sizeof(lapack_complex_float))
#define MAX_MBUFF (2 * MAX_BEAM * sizeof(int) + sizeof(float) + \
					  MAX_BEAM *CARRIER_NUM *SYMBOL_NUM * sizeof(lapack_complex_float))

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
