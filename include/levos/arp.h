#ifndef __LEVOS_ARP_H
#define __LEVOS_ARP_H

#include <levos/eth.h>
#include <levos/ip.h>
#include <levos/types.h>
#include <levos/kernel.h>
#include <levos/compiler.h>

#define ARP_OPCODE_REQUEST 0x0001
#define ARP_OPCODE_REPLY   0x0002

#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_HLEN_ETHERNET  0x0006

#define ARP_PTYPE_IP       0x0800
#define ARP_PLEN_IP        0x0004

struct arp_header {
    be_uint16_t arp_htype;
    be_uint16_t arp_ptype;
    uint8_t     arp_hlen;
    uint8_t     arp_plen;
    be_uint16_t arp_opcode;
    /* PAYLOAD HERE */
} __packed;

uint8_t arp_get_plen(uint16_t);
uint8_t arp_get_hlen(uint16_t);

int arp_write_header(struct arp_header *, uint16_t, uint8_t,
                 uint16_t, uint8_t, uint16_t, void *, void *, void *, void *);


int arp_add_header(packet_t *, uint16_t, uint16_t, uint8_t,
                void *, void *, void *, void *);


packet_t *arp_construct_packet(uint16_t, uint16_t, uint8_t,
        void *, void *, void *, void *);


packet_t *arp_construct_request(uint16_t, uint16_t, void *,
        void *, void *, void *);


packet_t *arp_construct_request_eth_ip(void *, ip_addr_t, ip_addr_t);

int arp_handle_packet(struct net_info *, packet_t *, struct arp_header *);

/* ARP cache */
int arp_cache_init(void);
int arp_cache_insert(ip_addr_t, eth_addr_t);
uint8_t *arp_get_eth_addr(struct net_info *, ip_addr_t);

#endif
