#include <stdio.h>
#include "info_pkg_head.h"

int check_info_pkg_head(info_pkg_head_t *h, info_pkg_head_t *ph)
{
    int ret;
    if (h->data_vld != 1)
        ret = 0;
    else
    {
        if (is_info_pkg_head_pre_right(h, ph) && is_pkg_len_legal(h->pack_len))
        {
            update_pre_info_pkg_head(h, ph);
            ret = 1;
        }
        else
        {
            init_pre_info_pkg_head(ph);
            ret = 0;
        }
    }
    return ret;
}

int is_info_pkg_head_pre_right(info_pkg_head_t *h, info_pkg_head_t *ph)
{
    if (h->pack_hd == 0x0B0E)
        return (h->pack_hd == ph->pack_hd) &&
               (h->pack_idx == ph->pack_idx) &&
               (h->pack_type >= 0x00) && (h->pack_type <= 0x04);
    else if (h->pack_hd == 0x0E0D)
        return (h->pack_type == ph->pack_type) &&
               (h->pack_idx == ph->pack_idx);
    else
        return (h->pack_type == ph->pack_type) &&
               (h->pack_hd == ph->pack_hd) &&
               (h->pack_idx == ph->pack_idx);
}

void init_pre_info_pkg_head(info_pkg_head_t *ph)
{
    ph->pack_type = 0xFF;
    ph->pack_hd = 0x0B0E;
    ph->pack_idx = 0x0000;
}

void update_pre_info_pkg_head(info_pkg_head_t *h, info_pkg_head_t *ph)
{
    if (h->pack_hd == 0x0E0D)
        init_pre_info_pkg_head(ph);
    else
    {
        ph->pack_type = h->pack_type;
        ph->pack_idx = h->pack_idx + 1;
        if (((ph->pack_type == 0x00) && (ph->pack_idx >= 0x0001)) ||
            ((ph->pack_type == 0x01) && (ph->pack_idx >= 0x0003)) ||
            ((ph->pack_type == 0x02) && (ph->pack_idx >= 0x0133)) ||
            ((ph->pack_type == 0x03) && (ph->pack_idx >= 0x0180)))
            ph->pack_hd = 0x0E0D;
        else
            ph->pack_hd = 0x000;
    }
}

int is_pkg_len_legal(int len)
{
    return len >= 0 && len <= 1400;
}

void write_data_pkg(info_pkg_head_t *h, uint8_t *adcnt)
{
    FILE *fp;
    switch (h->pack_type)
    {
    case 0x00:
        fp = fopen("USRP_state.txt", "a");
        break;
    case 0x01:
        fp = fopen("BEE7_state.txt", "a");
        break;
    case 0x02:
        fp = fopen("straight_data.txt", "a");
        break;
    case 0x03:
        fp = fopen("freq_data.txt", "a");
        break;
    default:
        fp = fopen("srs_data.txt", "a");
        break;
    }
    fclose(fp);
}