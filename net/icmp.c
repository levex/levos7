#include <levos/kernel.h>
#include <levos/icmp.h>
#include <levos/ip.h>
#include <levos/packet.h>
#include <levos/eth.h>
#include <levos/icmp.h>
#include <levos/e1000.h> /* FIXME: make it net_device eventually */

char *icmp_reply_data = "LevOS7hello!";

be_uint16_t
icmp_calculate_checksum(uint16_t *data, size_t len)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < len / sizeof(uint16_t); i ++)
        sum += data[i];

    if (i * sizeof(uint16_t) != len) {
        net_printk("stray byte\n");
        uint8_t *ptr = (uint8_t *)&data[i];
        sum += ptr[1];
    }

    while (sum > 0x0000ffff) {
        uint16_t val = (sum & 0xffff0000) >> 16;
        sum &= 0x0000ffff;
        sum += val;
    }

    return ~sum;
}

int
icmp_write_echo_reply(struct icmp_header *icmp,
        be_uint16_t icmp_seq, be_uint16_t icmp_id, void *data, size_t sz)
{
    struct icmp_echo_packet *echo = (void *)icmp + sizeof(*icmp);

    icmp->icmp_type = ICMP_TYPE_ECHO_REPLY;
    icmp->icmp_code = ICMP_CODE_ECHO_REPLY;
    icmp->icmp_chksum = 0;
    echo->icmp_echo_id = icmp_id;
    echo->icmp_echo_seq = icmp_seq;
    memcpy(((void *)&echo->icmp_echo_seq) + 2, data, sz);
    icmp->icmp_chksum = icmp_calculate_checksum((uint16_t *) icmp, sizeof(struct icmp_header) +
            sizeof(struct icmp_echo_packet) + sz);
}

int
icmp_send_echo_reply(struct net_info *ni, packet_t *pkt, struct icmp_header *icmp)
{
    struct ip_base_header *ip = pkt->p_buf + pkt->pkt_ip_offset;
    struct net_device *ndev = NDEV_FROM_NI(ni);
    struct icmp_echo_packet *echo = (void *)icmp + sizeof(*icmp);
    be_uint16_t icmp_seq = echo->icmp_echo_seq;
    be_uint16_t icmp_id = echo->icmp_echo_id;
    int datasz, dataoff;

    datasz = to_le_16(ip->ip_len)
        - sizeof(struct ip_base_header)
        - sizeof(struct icmp_header)
        - sizeof(struct icmp_echo_packet);

    dataoff = pkt->pkt_ip_offset
        + sizeof(struct ip_base_header)
        + sizeof(struct icmp_header)
        + sizeof(struct icmp_echo_packet);

    packet_t *tos = ip_construct_packet_eth(ni->ni_hw_mac, to_be_32(ni->ni_src_ip), to_be_32(ip->ip_srcaddr));
    if (!pkt)
        return PACKET_DROP;

    if (packet_grow(tos, sizeof(struct icmp_header) + sizeof(struct icmp_echo_packet) + datasz))
        return PACKET_DROP;

    icmp_write_echo_reply(tos->p_ptr, icmp_seq, icmp_id, pkt->p_buf + dataoff, datasz);

    net_printk(" ^ data length: %d, offset: %d\n", datasz, dataoff);

    ip_update_length(tos->p_buf + tos->pkt_ip_offset, sizeof(struct icmp_header)
                + sizeof(struct icmp_echo_packet) + datasz);
    ip_set_proto(tos->p_buf + tos->pkt_ip_offset, IP_PROTO_ICMP);

    /* swap the destination ethernet */
    memcpy(tos->p_buf, pkt->p_buf + 6, 6);

    ndev->send_packet(ndev, tos);

    return PACKET_HANDLED;
}

int
icmp_handle_echo_request(struct net_info *ni, packet_t *pkt, struct icmp_header *icmp)
{
    struct icmp_echo_packet *echo = (void *)icmp + sizeof(*icmp);

    net_printk("  ^ echo request seq %d id %d\n", to_le_16(echo->icmp_echo_seq),
                to_le_16(echo->icmp_echo_id));

    return icmp_send_echo_reply(ni, pkt, icmp);
}

int
icmp_handle_packet(struct net_info *ni, packet_t *pkt, struct icmp_header *icmp)
{
    net_printk("^ ICMP packet!\n");
    if (icmp->icmp_type == ICMP_TYPE_ECHO_REQUEST &&
            icmp->icmp_code == ICMP_CODE_ECHO_REQUEST) {
        return icmp_handle_echo_request(ni, pkt, icmp);
    }
    return PACKET_DROP;
}
