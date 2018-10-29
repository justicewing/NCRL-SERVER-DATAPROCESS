#include <stdio.h>
#include "info_pkg_head.h"

int check_info_pkg_head(info_pkg_head_t *h, info_pkg_head_t *ph)
{
    int ret;
    if (h->data_vld != 1)
        ret = 0;
    else
    {
        if ((is_info_pkg_head_pre_right(h, ph) ||
             is_info_pkg_head_a_legal_begin(h)) &&
            is_pkg_len_legal(h->pack_len))
        {
            update_pre_info_pkg_head(h, ph);
            ret = 1;
        }
        else
        {
            // if (is_info_pkg_head_a_legal_begin(h))
            // {
            //     update_pre_info_pkg_head(h, ph);
            //     ret = 2;
            // }
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
               (h->pack_idx == ph->pack_idx);
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
        if (((ph->pack_type == 0x00) && (ph->pack_idx >= 0x0003)) ||
            ((ph->pack_type == 0x01) && (ph->pack_idx >= 0x0133)) ||
            ((ph->pack_type == 0x02) && (ph->pack_idx >= 0x0180)) ||
            ((ph->pack_type >= 0xBA) && (ph->pack_idx >= 0x03E8)))
            ph->pack_hd = 0x0E0D;
        else
            ph->pack_hd = 0x0000;
    }
}

int is_pkg_len_legal(int len)
{
    return len >= 0 && len <= MAX_PKG_LEN;
}

int is_info_pkg_head_a_legal_begin(info_pkg_head_t *h)
{
    return h->pack_hd == 0x0B0E &&
           h->pack_idx == 0x0000 &&
           is_pkg_len_legal(h->pack_len);
}

void write_data_pkg(info_pkg_head_t *h, uint8_t *adcnt)
{
    FILE *fp;
    switch (h->pack_type)
    {
    case 0x00:
        fp = fopen("state.txt", "a");
        break;
    case 0x01:
        fp = fopen("straight_data.txt", "a");
        break;
    case 0x02:
        fp = fopen("freq_data.txt", "a");
        break;
    case 0x03:
        fp = fopen("srs_data.txt", "a");
        break;
    case 0xBA:
        fp = fopen("BEE7_blockA.txt", "a");
        break;
    case 0xBB:
        fp = fopen("BEE7_blockB.txt", "a");
        break;
    case 0xBC:
        fp = fopen("BEE7_blockC.txt", "a");
        break;
    case 0xBD:
        fp = fopen("BEE7_blockD.txt", "a");
        break;
    case 0xF0:
        fp = fopen("USRP_0.txt", "a");
        break;
    case 0xF1:
        fp = fopen("USRP_1.txt", "a");
        break;
    case 0xF2:
        fp = fopen("USRP_2.txt", "a");
        break;
    case 0xF3:
        fp = fopen("USRP_3.txt", "a");
        break;
    case 0xF4:
        fp = fopen("USRP_4.txt", "a");
        break;
    case 0xF5:
        fp = fopen("USRP_5.txt", "a");
        break;
    case 0xF6:
        fp = fopen("USRP_6.txt", "a");
        break;
    case 0xF7:
        fp = fopen("USRP_7.txt", "a");
        break;
    case 0xF8:
        fp = fopen("USRP_8.txt", "a");
        break;
    case 0xF9:
        fp = fopen("USRP_9.txt", "a");
        break;
    case 0xFA:
        fp = fopen("USRP_A.txt", "a");
        break;
    case 0xFB:
        fp = fopen("USRP_B.txt", "a");
        break;
    case 0xFC:
        fp = fopen("USRP_C.txt", "a");
        break;
    case 0xFD:
        fp = fopen("USRP_D.txt", "a");
        break;
    case 0xFE:
        fp = fopen("USRP_E.txt", "a");
        break;
    case 0xFF:
        fp = fopen("USRP_F.txt", "a");
        break;
    }
    wirte_data(fp, h, adcnt);
    fclose(fp);
}

void wirte_data(FILE *fp, info_pkg_head_t *h, uint8_t *adcnt)
{
    int i;
    uint16_t *p = (uint16_t *)adcnt;
    // fprintf(fp, "pack_type:%02X\t", h->pack_type);
    // fprintf(fp, "pack_hd:%02X%02X\t", h->pack_hd >> 8, h->pack_hd);
    // fprintf(fp, "pack_idx:%02X%02X\t", h->pack_idx >> 8, h->pack_idx);
    // fprintf(fp, "pack_len:%02X%02X\t", h->pack_len >> 8, h->pack_len);
    // fprintf(fp, "\n");
    for (i = 0; i < (h->pack_len + 1) / 2; i++)
        fprintf(fp, "%6d ", *(p++));
    fprintf(fp, "\n");
    // if (h->pack_hd == 0x0E0D)
    //     fprintf(fp, "\n");
}

void load_info_pkg_head_ul(info_pkg_head_t *h, uint8_t *adcnt)
{
    uint8_t *p = adcnt;
    *p = h->send_en;
    p++;
    *p = h->data_vld;
    p++;
    *p = h->pack_type;
    p++;
    *p = h->pack_hd >> 8;
    p++;
    *p = h->pack_hd;
    p++;
    *p = h->pack_idx >> 8;
    p++;
    *p = h->pack_idx;
    p++;
    *p = h->pack_len >> 8;
    p++;
    *p = h->pack_len;
    p++;
}

void load_info_pkg_head_dl(info_pkg_head_t *h, uint8_t *adcnt)
{
    uint8_t *p = adcnt;
    *p = h->pack_type;
    p++;
    *p = h->pack_hd >> 8;
    p++;
    *p = h->pack_hd;
    p++;
    *p = h->pack_idx >> 8;
    p++;
    *p = h->pack_idx;
    p++;
    *p = h->pack_len >> 8;
    p++;
    *p = h->pack_len;
    p++;
}

void init_dl_info_pkg_head(info_pkg_head_t *h)
{
    h->pack_type = 0x00;
    h->pack_hd = 0x0B0E;
    h->pack_idx = 0x0000;
    h->pack_len = MAX_PKG_LEN; // 0x0577
}

void init_ul_info_pkg_head(info_pkg_head_t *h)
{
    h->send_en = 0x01;
    h->data_vld = 0x01;
    h->pack_type = 0x00;
    h->pack_hd = 0x0B0E;
    h->pack_idx = 0x0000;
    h->pack_len = MAX_PKG_LEN; // 0x0577
}