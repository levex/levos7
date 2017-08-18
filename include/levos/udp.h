#ifndef __LEVOS_UDP_H
#define __LEVOS_UDP_H

#include <levos/compiler.h>
#include <levos/types.h>
#include <levos/ip.h>
#include <levos/packet.h>
#include <levos/ring.h>
#include <levos/hash.h>
#include <levos/list.h>

struct udp_header {
    be_port_t   udp_src_port;
    be_port_t   udp_dst_port;
    be_uint16_t udp_len;
    be_uint16_t udp_chksum;
} __packed;


#define UDP_BUFFER_SIZE 4096

/* represents a single UDP datagram */
struct udp_dgram {
    void *udg_buffer;
    size_t udg_len;

    struct list_elem udg_elem;
};

struct udp_sock_priv {
    ip_addr_t usp_dstip;
    port_t    usp_dstport;
    port_t    usp_srcport;

    struct hash_elem usp_helem;

    /* total amount of space used by the datagrams */
    size_t usp_buffer_len;

    /* list of struct udp_dgram */
    struct list usp_dgrams;
};

extern struct socket_ops udp_sock_ops;

packet_t *
udp_new_packet(struct net_info *, port_t, ip_addr_t, port_t);

packet_t *
udp_construct_packet(struct net_info *, eth_addr_t, ip_addr_t, port_t, ip_addr_t, port_t);

int udp_set_payload(packet_t *, void *, size_t);

int udp_handle_packet(struct net_info *, packet_t *, struct udp_header *);

#endif
