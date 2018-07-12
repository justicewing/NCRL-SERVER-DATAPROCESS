#ifndef INFO_PKG_HEAD_H
#define INFO_PKG_HEAD_H

#include <stdint.h>

typedef struct
{
    uint8_t data_vld;
    uint8_t pack_type;
    uint16_t pack_hd;
    uint16_t pack_idx;
    uint16_t pack_len;
} info_pkg_head_t;

int check_info_pkg_head(info_pkg_head_t* h, info_pkg_head_t* ph);

int is_info_pkg_head_pre_right(info_pkg_head_t* h, info_pkg_head_t* ph);

void init_pre_info_pkg_head(info_pkg_head_t* ph);

void update_pre_info_pkg_head(info_pkg_head_t* h, info_pkg_head_t* ph);

int is_pkg_len_legal(int len);

void write_data_pkg(info_pkg_head_t* h,uint8_t* adcnt);

#endif