#include <stdint.h>

#include "config.h"

#ifndef PC_CBSEGM_H
#define PC_CBSEGM_H

typedef struct
{
    uint32_t I_seg;
    uint32_t C;
    uint32_t tbs;
} seu5g_pc_cbsegm_t;

int seu5g_pc_cbsegm(seu5g_pc_cbsegm_t *s, uint32_t tbs, uint32_t I_seg);

#endif