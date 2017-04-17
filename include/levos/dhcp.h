#ifndef __LEVOS_DHCP_H
#define __LEVOS_DHCP_H

#include <levos/types.h>
#include <levos/eth.h>

#define DHCP_SOURCE_PORT 68
#define DHCP_DEST_PORT 67

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_OP_DISCOVER 0x01
#define DHCP_OP_OFFER    0x02
#define DHCP_OP_REQUEST  0x01
#define DHCP_OP_ACK      0x02

#define DHCP_HTYPE_ETH   0x01
#define DHCP_HLEN_ETH    0x06

struct dhcp_packet {
    uint8_t    dhcp_op;
    uint8_t    dhcp_htype;
    uint8_t    dhcp_hlen;
    uint8_t    dhcp_hops;
    uint32_t   dhcp_xid;
    uint16_t   dhcp_secs;
    uint16_t   dhcp_flags;
    uint32_t   dhcp_ciaddr;
    uint32_t   dhcp_yiaddr;
    uint32_t   dhcp_siaddr;
    uint32_t   dhcp_giaddr;
    eth_addr_t dhcp_chaddr;
    uint8_t    dhcp_reserved[10];
    char       dhcp_servername[64];
    char       dhcp_bootfilename[128];
    uint32_t   dhcp_cookie;
} __packed;


int dhcp_handle_packet(struct net_info *, packet_t *, struct udp_header *);

void dhcp_start_discovery(struct net_device *, struct net_info *);

void do_dhcp(struct net_device *);

#endif /* __LEVOS_DHCP_H */
