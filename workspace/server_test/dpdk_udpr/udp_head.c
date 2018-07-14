#include "udp_head.h"

void load_udp_head(mac_hdr *mhdr, ip_hdr *ihdr, udp_fhdr_hdr *uhdr, uint8_t *adcnt)
{
    // unsigned short ichecksum = cal_ip_checksum(ihdr);
    // unsigned short uchecksum = cal_udp_checksum(uhdr, data);
    unsigned short ichecksum = 0;
    unsigned short uchecksum = 0;
    *adcnt = mhdr->dst_mac[0];
    adcnt++;
    *adcnt = mhdr->dst_mac[1];
    adcnt++;
    *adcnt = mhdr->dst_mac[2];
    adcnt++;
    *adcnt = mhdr->dst_mac[3];
    adcnt++;
    *adcnt = mhdr->dst_mac[4];
    adcnt++;
    *adcnt = mhdr->dst_mac[5];
    adcnt++;

    *adcnt = mhdr->src_mac[0];
    adcnt++;
    *adcnt = mhdr->src_mac[1];
    adcnt++;
    *adcnt = mhdr->src_mac[2];
    adcnt++;
    *adcnt = mhdr->src_mac[3];
    adcnt++;
    *adcnt = mhdr->src_mac[4];
    adcnt++;
    *adcnt = mhdr->src_mac[5];
    adcnt++;
    *adcnt = mhdr->type0;
    adcnt++;
    *adcnt = mhdr->type1;
    adcnt++;

    *adcnt = ihdr->version_headlen_tos >> 8;
    adcnt++;
    *adcnt = ihdr->version_headlen_tos;
    adcnt++;
    *adcnt = ihdr->length >> 8;
    adcnt++;
    *adcnt = ihdr->length;
    adcnt++;
    *adcnt = ihdr->flags0 >> 8;
    adcnt++;
    *adcnt = ihdr->flags0;
    adcnt++;
    *adcnt = ihdr->flags1 >> 8;
    adcnt++;
    *adcnt = ihdr->flags1;
    adcnt++;
    *adcnt = ihdr->ttl_protocol_checksum0 >> 8;
    adcnt++;
    *adcnt = ihdr->ttl_protocol_checksum0;
    adcnt++;
    *adcnt = ichecksum >> 8;
    adcnt++;
    *adcnt = ichecksum;
    adcnt++;
    *adcnt = ihdr->src_ip0 >> 8;
    adcnt++;
    *adcnt = ihdr->src_ip0;
    adcnt++;
    *adcnt = ihdr->src_ip1 >> 8;
    adcnt++;
    *adcnt = ihdr->src_ip1;
    adcnt++;
    *adcnt = ihdr->dst_ip0 >> 8;
    adcnt++;
    *adcnt = ihdr->dst_ip0;
    adcnt++;
    *adcnt = ihdr->dst_ip1 >> 8;
    adcnt++;
    *adcnt = ihdr->dst_ip1;
    adcnt++;

    *adcnt = uhdr->src_port >> 8;
    adcnt++;
    *adcnt = uhdr->src_port;
    adcnt++;
    *adcnt = uhdr->dst_port >> 8;
    adcnt++;
    *adcnt = uhdr->dst_port;
    adcnt++;
    *adcnt = uhdr->length >> 8;
    adcnt++;
    *adcnt = uhdr->length;
    adcnt++;
    *adcnt = uchecksum >> 8;
    adcnt++;
    *adcnt = uchecksum;
    adcnt++;
}

void init_mac_hdr(mac_hdr *mhdr)
{
    mhdr->dst_mac[0] = 0xA0;
    mhdr->dst_mac[1] = 0x36;
    mhdr->dst_mac[2] = 0x7A;
    mhdr->dst_mac[3] = 0x58;
    mhdr->dst_mac[4] = 0xAD;
    mhdr->dst_mac[5] = 0x76;
    // mhdr->dst_mac[0] = 0xFF;
    // mhdr->dst_mac[1] = 0xFF;
    // mhdr->dst_mac[2] = 0xFF;
    // mhdr->dst_mac[3] = 0xFF;
    // mhdr->dst_mac[4] = 0xFF;
    // mhdr->dst_mac[5] = 0xFF;

    mhdr->src_mac[0] = 0xA0;
    mhdr->src_mac[1] = 0x36;
    mhdr->src_mac[2] = 0x9F;
    mhdr->src_mac[3] = 0x58;
    mhdr->src_mac[4] = 0xA6;
    mhdr->src_mac[5] = 0x76;
    // mhdr->src_mac[0] = 0xFF;
    // mhdr->src_mac[1] = 0xFF;
    // mhdr->src_mac[2] = 0xFF;
    // mhdr->src_mac[3] = 0xFF;
    // mhdr->src_mac[4] = 0xFF;
    // mhdr->src_mac[5] = 0xFF;
    mhdr->type0 = 0x08;
    mhdr->type1 = 0x00;
}

void init_ip_hdr(ip_hdr *ihdr, uint16_t len)
{
    ihdr->version_headlen_tos = 0x4500;
    ihdr->length = len + IP_HEAD_LEN;

    ihdr->flags0 = 0x0000;
    ihdr->flags1 = 0x4000;

    ihdr->ttl_protocol_checksum0 = 0x4011;
    ihdr->ttl_protocol_checksum1 = 0x0000;

    ihdr->src_ip0 = 0xC0A8;
    ihdr->src_ip1 = 0x1405;

    ihdr->dst_ip0 = 0xC0A8;
    ihdr->dst_ip1 = 0x1401;
}

void init_udp_fhdr_hdr(udp_fhdr_hdr *uhdr, uint16_t len)
{
    uhdr->src_port = 0xBEE4;
    uhdr->dst_port = 0xBEE4;
    uhdr->length = len + UDP_HEAD_LEN;
    uhdr->reserved = 0x0011;
    uhdr->src_ip0 = 0xC0A8;
    uhdr->src_ip1 = 0x1405;

    uhdr->dst_ip0 = 0xC0A8;
    uhdr->dst_ip1 = 0x1401;
}

unsigned short cal_ip_checksum(ip_hdr *hdr)
{
    unsigned long checksum = 0;
    checksum =
        hdr->version_headlen_tos + hdr->length + hdr->flags0 + hdr->flags1 + hdr->ttl_protocol_checksum0 + hdr->ttl_protocol_checksum1 + hdr->src_ip0 + hdr->src_ip1 + hdr->dst_ip0 + hdr->dst_ip1;
    checksum = (checksum >> 16) + (checksum & 0xffff);
    checksum += (checksum >> 16);
    return (unsigned short)(~checksum);
}
unsigned short cal_udp_checksum(udp_fhdr_hdr *hdr, unsigned char *buffer)
{
    unsigned long checksum = 0;
    int num = hdr->length - 8;
    checksum =
        hdr->src_ip0 + hdr->src_ip1 + hdr->dst_ip0 + hdr->dst_ip1 + hdr->reserved + hdr->length + hdr->src_port + hdr->dst_port + hdr->length + hdr->checksum;
    while (num > 1)
    {
        checksum += (*buffer << 8) + *(buffer + 1);
        buffer += 2;
        num -= sizeof(short);
    }
    if (num)
    {
        checksum += *(unsigned short *)buffer << 8;
    }
    checksum = (checksum >> 16) + (checksum & 0xffff);
    checksum += (checksum >> 16);
    return (unsigned short)(~checksum);
}
unsigned short get_checksum(unsigned short *buffer, int size) //size是字节数
{
    unsigned long cksum = 0;
    while (size > 1)
    {
        printf("size=%d  buffer=%x\n", size, *buffer);
        cksum += *buffer;
        buffer++;
        size -= sizeof(short);
        printf("size=%d  checksum=%x\n", size, cksum);
    }
    if (size)
    {
        cksum += *(unsigned short *)buffer << 8;
        printf("size=%d  buffer=%x\n", 1, *buffer);
        printf("size=%d  checksum=%x\n", size, cksum);
    }
    printf("checksum00=%x\n", cksum);
    cksum = (cksum >> 16) + (cksum & 0xffff);
    printf("+ checksum11=%x\n", cksum);
    cksum += (cksum >> 16);
    printf("= checksum22=%x\n", cksum);
    return (unsigned short)(~cksum);
}