#ifndef __LEVOS_ICMP_H
#define __LEVOS_ICMP_H

#include <levos/types.h>
#include <levos/compiler.h>
#include <levos/packet.h>

#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_CODE_ECHO_REPLY 0

#define ICMP_TYPE_ECHO_REQUEST 8
#define ICMP_CODE_ECHO_REQUEST 0

struct icmp_header {
    uint8_t icmp_type;
    uint8_t icmp_code;
    be_uint16_t icmp_chksum;
} __packed;

struct icmp_echo_packet {
    be_uint16_t icmp_echo_id;
    be_uint16_t icmp_echo_seq;
} __packed;


int icmp_handle_packet(struct net_info *, packet_t *, struct icmp_header *);

#endif /* __LEVOS_ICMP_H */
