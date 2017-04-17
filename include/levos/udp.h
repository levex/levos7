#ifndef __LEVOS_UDP_H
#define __LEVOS_UDP_H

#include <levos/compiler.h>
#include <levos/types.h>
#include <levos/ip.h>
#include <levos/packet.h>

struct udp_header {
    be_port_t   udp_src_port;
    be_port_t   udp_dst_port;
    be_uint16_t udp_len;
    be_uint16_t udp_chksum;
} __packed;

struct udp_sock_priv {
    ip_addr_t usp_dstip;
    port_t    usp_dstport;
    port_t    usp_srcport;
};

extern struct socket_ops udp_sock_ops;

packet_t *
udp_new_packet(struct net_info *, port_t, ip_addr_t, port_t);

packet_t *
udp_construct_packet(struct net_info *, eth_addr_t, ip_addr_t, port_t, ip_addr_t, port_t);

int udp_set_payload(packet_t *, void *, size_t);

int udp_handle_packet(struct net_info *, packet_t *, struct udp_header *);

#endif
