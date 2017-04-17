#ifndef __LEVOS_ETH_H
#define __LEVOS_ETH_H

#include <levos/types.h>
#include <levos/packet.h>
#include <levos/compiler.h>

typedef be_uint8_t eth_addr_t[6];


#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP4 0x0800

struct ethernet_header {
    be_uint8_t  eth_dst[6];
    be_uint8_t  eth_src[6];
    be_uint16_t eth_type;
} __packed;

inline uint16_t
eth_get_proto(struct ethernet_header *eth)
{
    return to_le_16(eth->eth_type);
}

extern eth_addr_t eth_broadcast_addr;
extern eth_addr_t eth_null_addr;

void eth_dump_packet(packet_t *);

packet_t *eth_construct_packet(eth_addr_t, eth_addr_t, uint16_t);

int eth_should_drop(struct net_info *, struct ethernet_header *);

/* %pE format specifier */
void printk_print_eth_addr(eth_addr_t);

#endif /* __LEVOS_ETH_H */
