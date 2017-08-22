#ifndef __LEVOS_TCP_H
#define __LEVOS_TCP_H

#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/packet.h>
#include <levos/ip.h>
#include <levos/hash.h>
#include <levos/ring.h>

#define TCP_FLAGS_NS   (1 << 8)
#define TCP_FLAGS_CWR  (1 << 7)
#define TCP_FLAGS_ECE  (1 << 6)
#define TCP_FLAGS_URG  (1 << 5)
#define TCP_FLAGS_ACK  (1 << 4)
#define TCP_FLAGS_PSH  (1 << 3)
#define TCP_FLAGS_RST  (1 << 2)
#define TCP_FLAGS_SYN  (1 << 1)
#define TCP_FLAGS_FIN  (1 << 0)

struct tcp_header {
    be_port_t   tcp_src_port;
    be_port_t   tcp_dst_port;
    be_uint32_t tcp_seq;
    be_uint32_t tcp_ack;
    be_uint16_t tcp_doff_flags;
    be_uint16_t tcp_wsize;
    be_uint16_t tcp_chksum;
    be_uint16_t tcp_urgp;
} __packed;

packet_t *tcp_new_packet(struct net_info *, port_t, ip_addr_t, port_t);
void tcp_finalize_packet(struct tcp_header *, size_t);
int tcp_set_payload(packet_t *, void *, size_t);

inline uint8_t
tcp_get_doff(struct tcp_header *tcp)
{
    uint16_t doff_flags = to_le_16(tcp->tcp_doff_flags);
    return (doff_flags & 0xf000) >> 12;
}

inline void
tcp_set_doff(struct tcp_header *tcp, uint8_t doff)
{
    uint16_t local = to_le_16(tcp->tcp_doff_flags);
    local &= 0x0fff;
    local |= (doff & 0x0f) << 12;
    tcp->tcp_doff_flags = to_be_16(local);
}

inline be_uint16_t
tcp_get_flags(struct tcp_header *tcp)
{
    uint16_t doff_flags = to_le_16(tcp->tcp_doff_flags);
    return (doff_flags & 0x1f);
}

#define _PASTE(x, y) x ## y
#define PASTE(x, y) _PASTE(x, y)

#define _gen_tcp_flag_getset(lc, hc) \
    inline int \
    tcp_is_set_##lc (struct tcp_header *tcp) \
    { \
        return (tcp_get_flags(tcp)) & PASTE(TCP_FLAGS_, hc); \
    } \
    inline void \
    tcp_set_##lc (struct tcp_header *tcp, int bit) \
    { \
        if (bit) \
            tcp->tcp_doff_flags = to_be_16(to_le_16(tcp->tcp_doff_flags) | PASTE(TCP_FLAGS_, hc)); \
        else \
            tcp->tcp_doff_flags = to_be_16(to_le_16(tcp->tcp_doff_flags) & ~(PASTE(TCP_FLAGS_, hc))); \
    } \
    inline void \
    tcp_packet_set_##lc (packet_t *pkt, int bit) \
    { \
        struct tcp_header *tcp = pkt->p_buf + pkt->pkt_proto_offset; \
        tcp_set_##lc (tcp, bit); \
    } \
    inline int \
    tcp_packet_is_set_##lc (packet_t *pkt) \
    { \
        struct tcp_header *tcp = pkt->p_buf + pkt->pkt_proto_offset; \
        return tcp_is_set_##lc (tcp); \
    }

_gen_tcp_flag_getset(ns,   NS);
_gen_tcp_flag_getset(cwr, CWR);
_gen_tcp_flag_getset(ece, ECE);
_gen_tcp_flag_getset(urg, URG);
_gen_tcp_flag_getset(ack, ACK);
_gen_tcp_flag_getset(psh, PSH);
_gen_tcp_flag_getset(rst, RST);
_gen_tcp_flag_getset(syn, SYN);
_gen_tcp_flag_getset(fin, FIN);

#define TI_STATE_CLOSED      0
#define TI_STATE_SYN_SENT    1
#define TI_STATE_ESTAB       2
#define TI_STATE_CLOSE_WAIT  3
#define TI_STATE_LAST_ACK    4
#define TI_STATE_LISTEN      5
#define TI_STATE_SYN_RCVD    6
#define TI_STATE_FIN_WAIT_1  7
#define TI_STATE_FIN_WAIT_2  8
#define TI_STATE_TIME_WAIT   9
#define TI_STATE_CLOSING    10

struct tcp_info {
             port_t           ti_src_port; /* port on our machine */
             port_t           ti_dst_port; /* port on remote machine */
    volatile int              ti_tcp_state;
             int              ti_fail_code;
             uint32_t         ti_next_ack; /* how much of the other side we've seen so far */
             uint32_t         ti_next_seq; /* how much we've sent so far */

             ip_addr_t        ti_dstip;

             struct work     *ti_retransmit_work;

#define TCP_BUFFER_SIZE 16384
             struct ring_buffer ti_rb; /* data collected so far */
             
             struct hash_elem ti_helem;
};

void test_tcp(struct net_info *);

bool tcp_less_tcp_info(const struct hash_elem *,
                     const struct hash_elem *,
                     void *);
unsigned tcp_hash_tcp_info(const struct hash_elem *, void *);

int tcp_handle_packet(struct net_info *, packet_t *, struct tcp_header *);

#endif /* __LEVOS_TCP_H */
