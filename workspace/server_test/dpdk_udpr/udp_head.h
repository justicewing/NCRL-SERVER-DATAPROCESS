#ifndef UDP_HEAD_H
#define UDP_HEAD_H

#include <stdint.h>

#define UDP_HEAD_LEN 8
#define IP_HEAD_LEN 20
#define MAC_HEAD_LEN 14

typedef struct
{
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint8_t type0;
    uint8_t type1;
} mac_hdr;
typedef struct
{

    uint16_t version_headlen_tos;
    uint16_t length; //ip包总长度，包含ip头

    uint16_t flags0;
    uint16_t flags1; //标识-标志-片偏移

    uint16_t ttl_protocol_checksum0;
    uint16_t ttl_protocol_checksum1; //ttl-协议-校验

    uint16_t src_ip0;
    uint16_t src_ip1; //源ip

    uint16_t dst_ip0;
    uint16_t dst_ip1; //目的ip
} ip_hdr;
typedef struct
{

    uint16_t src_ip0;
    uint16_t src_ip1; //源ip
    uint16_t dst_ip0;
    uint16_t dst_ip1; //目的ip
    uint16_t reserved;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_fhdr_hdr;

uint16_t cal_ip_checksum(ip_hdr *hdr);

uint16_t cal_udp_checksum(udp_fhdr_hdr *hdr, uint8_t *buffer);

uint16_t get_checksum(uint16_t *buffer, int size); //size是字节数

void load_udp_head(mac_hdr *mhdr, ip_hdr *ihdr, udp_fhdr_hdr *uhdr, uint8_t *adcnt);

void init_mac_hdr(mac_hdr *mhdr);

void init_ip_hdr(ip_hdr *ihdr, uint16_t len);

void init_udp_fhdr_hdr(udp_fhdr_hdr *uhdr, uint16_t len);

#endif