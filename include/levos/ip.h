#ifndef __LEVOS_IP4_H
#define __LEVOS_IP4_H

#include <levos/types.h>
#include <levos/eth.h>
#include <levos/compiler.h>

typedef uint32_t ip_addr_t;
typedef be_uint32_t be_ip_addr_t;

#define IP(a,b,c,d) ((ip_addr_t)(a << 24 | b << 16 | c << 8 | d))

#define IP_FLAGS_RS (1 << 0) /* Reserved, must be zero */
#define IP_FLAGS_DF (1 << 1) /* Don't fragment */
#define IP_FLAGS_MF (1 << 2) /* More fragments */


#define IP_PROTO_HOPOPT 0x00
#define IP_PROTO_ICMP   0x01
#define IP_PROTO_IGMP   0x02
#define IP_PROTO_GGP    0x03
#define IP_PROTO_IPINIP 0x04
#define IP_PROTO_ST     0x05
#define IP_PROTO_TCP    0x06
/* etc. etc. etc. */
#define IP_PROTO_UDP    0x11

struct ip_base_header {
    uint8_t      ip_ver_ihl;
    uint8_t      ip_dscp_ecn;
    uint16_t     ip_len;
    uint16_t     ip_ident;
    uint16_t     ip_flags_fr_off;
    uint8_t      ip_ttl;
    uint8_t      ip_proto;
    uint16_t     ip_chksum;
    be_ip_addr_t ip_srcaddr;
    be_ip_addr_t ip_dstaddr;
} __packed;

inline uint8_t
ip_get_version(struct ip_base_header *ip)
{
    return (ip->ip_ver_ihl & 0xf0) >> 4;
}

inline void
ip_set_version(struct ip_base_header *ip, uint8_t ver)
{
    /* clear top 4 first bytes */
    ip->ip_ver_ihl &= 0x0f;
    ip->ip_ver_ihl |= (ver << 4);
}

inline void
ip_set_ihl(struct ip_base_header *ip, uint8_t ihl)
{
    /* clear bottom 4 bytes */
    ip->ip_ver_ihl &= 0xf0;
    ip->ip_ver_ihl |= ihl & 0x0f;
}

inline void
ip_set_dscp(struct ip_base_header *ip, uint8_t dscp)
{
    /* clear top 6 bytes */
    ip->ip_dscp_ecn &= ~0xfc;
    ip->ip_dscp_ecn|= (dscp & 0x3f) << 2;
}

inline void
ip_set_ecn(struct ip_base_header *ip, uint8_t ecn)
{
    /* clear bottom 2 bytes */
    ip->ip_dscp_ecn &= 0xfc;
    ip->ip_dscp_ecn |= (ecn & 0x03);
}

inline void
ip_set_flags(struct ip_base_header *ip, uint8_t flags)
{
    /* clear top 3 bytes */
    ip->ip_flags_fr_off &= 0xe000;
    ip->ip_flags_fr_off |= ((flags & 0x3) << 13);
}

inline void
ip_set_fr_off(struct ip_base_header *ip, uint16_t fr_off)
{
    /* clear bottom 16 bytes */
    ip->ip_flags_fr_off &= ~0xe000;
    ip->ip_flags_fr_off |= (fr_off & 0x1fff);
}


packet_t *ip_construct_packet(ip_addr_t, ip_addr_t);

packet_t *ip_construct_packet_eth(eth_addr_t, be_ip_addr_t, be_ip_addr_t);
packet_t *ip_construct_packet_ni(struct net_info *, ip_addr_t);
packet_t *ip_construct_packet_eth_full(eth_addr_t, eth_addr_t, be_ip_addr_t,
        be_ip_addr_t);

int ip_add_header(packet_t *, be_ip_addr_t, be_ip_addr_t);

void ip_write_header(struct ip_base_header *, be_ip_addr_t, be_ip_addr_t);

be_uint16_t ip_calculate_checksum(struct ip_base_header *);
void ip_update_length(struct ip_base_header *, size_t);
void ip_set_proto(struct ip_base_header *, uint16_t);

int ip_handle_packet(struct net_info *, packet_t *, struct ip_base_header *);

void printk_print_ip_addr(uint32_t);

int ipcmp(ip_addr_t, ip_addr_t);

#endif /* __LEVOS_IP4_H */

