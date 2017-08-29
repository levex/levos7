#include <levos/kernel.h>
#include <levos/packet.h>
#include <levos/arp.h>
#include <levos/eth.h>
#include <levos/tcp.h>
#include <levos/udp.h>
#include <levos/icmp.h>

void
printk_print_ip_addr(uint32_t _ip)
{
    char *ip = (char *) &_ip;
    printk("%d.%d.%d.%d", (uint8_t) ip[0], (uint8_t) ip[1], (uint8_t) ip[2], (uint8_t) ip[3]);
}

void
printk_print_le_ip_addr(uint32_t _ip)
{
    char *ip = (char *) &_ip;
    printk("%d.%d.%d.%d", (uint8_t) ip[3], (uint8_t) ip[2], (uint8_t) ip[1], (uint8_t) ip[0]);
}

be_uint16_t
ip_calculate_checksum(struct ip_base_header *ip)
{
    uint16_t *buffer = (uint16_t *) ip;
    uint32_t sum = 0;
    int i;
    int len = 20; /* FIXME: get it from ip_ihl */

    for (i = 0; i < len / sizeof(uint16_t); i ++)
        sum += (buffer[i]);

    while (sum > 0xffff) {
        uint16_t val = (sum & 0xffff0000) >> 16;
        sum &= 0x0000ffff;
        sum += val;
    }

    sum = ~sum;

    return (sum);
}

void
ip_update_length(struct ip_base_header *ip, size_t len)
{
    ip->ip_len = to_be_16(20 + len);

    ip->ip_chksum = 0;
    ip->ip_chksum = ip_calculate_checksum(ip);
}

void
ip_set_proto(struct ip_base_header *ip, uint16_t proto)
{
    ip->ip_proto = proto;

    ip->ip_chksum = 0;
    ip->ip_chksum = ip_calculate_checksum(ip);
}

void
ip_write_header(struct ip_base_header *ip, be_ip_addr_t src, be_ip_addr_t dst)
{
    ip_set_version(ip, 4);
    ip_set_ihl(ip, 5);
    ip_set_dscp(ip, 0);
    ip_set_ecn(ip, 0);

    ip->ip_len = to_be_16(20);
    ip->ip_ident = to_be_16(0);

    ip_set_flags(ip, 0);
    ip_set_fr_off(ip, 0);

    ip->ip_ttl = 64;
    ip->ip_proto = IP_PROTO_TCP;
    ip->ip_srcaddr = to_le_32(src);
    ip->ip_dstaddr = to_le_32(dst);
    ip->ip_chksum = 0;
    ip->ip_chksum = ip_calculate_checksum(ip);
}

int
ip_add_header(packet_t *pkt, be_ip_addr_t src, be_ip_addr_t dst)
{
    /* grow the packet to have space for the IP header */
    if (packet_grow(pkt, sizeof(struct ip_base_header)))
        return -ENOMEM;

    ip_write_header(pkt->p_ptr, src, dst);

    return 0;
}

packet_t *
ip_construct_packet_eth_full(eth_addr_t srceth, eth_addr_t dsteth, be_ip_addr_t src,
        be_ip_addr_t dst)
{
    packet_t *pkt;
    int rc;

    pkt = eth_construct_packet(srceth, dsteth, ETH_TYPE_IP4);
    if (!pkt)
        return NULL;

    //eth_dump_packet(pkt);

    rc = ip_add_header(pkt, src, dst);
    if (rc)
        return NULL;

    pkt->pkt_ip_offset = pkt->p_ptr - pkt->p_buf;

    return pkt;
}

packet_t *
ip_construct_packet_eth(eth_addr_t srceth, be_ip_addr_t src, be_ip_addr_t dst)
{
    /* FIXME! */
    return ip_construct_packet_eth_full(srceth, eth_broadcast_addr, src, dst);
}

packet_t *
ip_construct_packet_ni(struct net_info *ni, ip_addr_t dst)
{
    packet_t *pkt;
    uint8_t *desteth;

    /* do the routing */
    desteth = net_route_to(ni, dst);
    if (desteth == NULL) {
        printk("null eth\n");
        return NULL;
    }
    
    pkt = ip_construct_packet_eth_full(ni->ni_hw_mac, desteth, ni->ni_src_ip, dst);
    if (!pkt) {
        printk("failed to construct ip packet\n");
        return NULL;
    }

    return pkt;
}

packet_t *
ip_construct_packet(ip_addr_t _src, ip_addr_t _dst)
{
    be_ip_addr_t src = to_be_32(_src);
    be_ip_addr_t dst = to_be_32(_dst);

    return ip_construct_packet_eth(eth_broadcast_addr, src, dst);
}

int
ipcmp(ip_addr_t a, ip_addr_t b)
{
    return a != b;
}

int
ip_should_drop(struct net_info *ni, struct ip_base_header *ip)
{
    /* if the IP is us, then don't drop */
    if (ipcmp(ni->ni_src_ip, to_le_32(ip->ip_dstaddr)) == 0)
        return 0;

    /* don't drop broadcast packets */
    if (ipcmp(ip->ip_dstaddr, IP(255, 255, 255, 255)) == 0)
        return 0;

    return 1;
}

int
ip_handle_packet(struct net_info *ni, packet_t *pkt, struct ip_base_header *ip)
{
    if (ip_get_version(ip) != 4) {
        net_printk(" ^ wrong ip version\n");
        return PACKET_DROP;
    }

    if (ip_should_drop(ni, ip)) {
        net_printk(" ^ not addressed to us\n");
        return PACKET_DROP;
    }

    pkt->pkt_ip_offset = (int)ip - (int)pkt->p_buf;
    pkt->p_ptr += sizeof(struct ip_base_header);
    if (ip->ip_proto == IP_PROTO_UDP) {
        return udp_handle_packet(ni, pkt, pkt->p_ptr);
    } else if (ip->ip_proto == IP_PROTO_ICMP) {
        return icmp_handle_packet(ni, pkt, pkt->p_ptr);
    } else if (ip->ip_proto == IP_PROTO_TCP) {
        return tcp_handle_packet(ni, pkt, pkt->p_ptr);
    }

    net_printk(" ^ unknown IP proto, drop\n");

    /* couldn't handle */
    return PACKET_COULDNTHANDLE;
}
