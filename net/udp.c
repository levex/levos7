#include <levos/kernel.h>
#include <levos/packet.h>
#include <levos/udp.h>
#include <levos/ip.h>
#include <levos/eth.h>
#include <levos/arp.h>
#include <levos/dhcp.h>
#include <levos/work.h>
#include <levos/socket.h>

unsigned udp_hash_usp(const struct hash_elem *e, void *aux)
{
    struct udp_sock_priv *usp = hash_entry(e, struct udp_sock_priv, usp_helem);

    return hash_int(usp->usp_srcport);
}

bool udp_less_usp(const struct hash_elem *a,
                     const struct hash_elem *b,
                     void *aux)
{
    struct udp_sock_priv *ta = hash_entry(a, struct udp_sock_priv, usp_helem);
    struct udp_sock_priv *tb = hash_entry(b, struct udp_sock_priv, usp_helem);

    return ta->usp_srcport < ta->usp_srcport;
}

void
udp_write_header(struct udp_header *udp, port_t srcport, port_t dstport)
{
    udp->udp_src_port = to_be_16(srcport);
    udp->udp_dst_port = to_be_16(dstport);
    udp->udp_len = to_be_16(8); /* minimum */
    udp->udp_chksum = 0; /* TODO: do checksum */
}

uint16_t
udp_calc_checksum(struct ip_base_header *ip, struct udp_header *udp, void *payload, size_t payload_len)
{
    uint32_t sum = 0;
    int i = 0;

    uint16_t *ptr = (uint16_t *) &ip->ip_srcaddr;
    sum += ptr[0];
    sum += ptr[1];

    ptr = (uint16_t *) &ip->ip_dstaddr;
    sum += ptr[0];
    sum += ptr[1];

    sum += 0;
    sum += ip->ip_proto;

    sum += udp->udp_len;

    sum += udp->udp_src_port;
    sum += udp->udp_dst_port;

    sum += udp->udp_len;

    ptr = payload;
    for (i = 0; i < payload_len / sizeof(uint16_t); i++)
        sum += ptr[i];

    while (sum > 0xffff) {
        uint16_t val = sum & 0xffff0000;
        sum &= 0x0000ffff;
        sum += val;
    }

    return sum;
}

int udp_add_header(packet_t *pkt, port_t srcport, port_t dstport)
{
    if (packet_grow(pkt, sizeof(struct udp_header)))
        return -ENOMEM;

    udp_write_header(pkt->p_ptr, srcport, dstport);

    return 0;
}

int
udp_set_payload(packet_t *pkt, void *data, size_t len)
{
    struct udp_header *udp;
    struct ip_base_header *ip;

    /* first, grow the packet */
    if (packet_grow(pkt, len))
        return -ENOMEM;

    /* add the payload */
    memcpy(pkt->p_ptr, data, len);

    /* update UDP header */
    udp = pkt->p_buf + pkt->pkt_proto_offset;
    udp->udp_len = to_be_16(sizeof(struct udp_header) + len);
    //udp->udp_chksum = udp_calc_checksum(pkt->p_buf + pkt->pkt_ip_offset, udp, data, len);
    udp->udp_chksum = 0;

    /* update the IP header */
    ip = pkt->p_buf + pkt->pkt_ip_offset;
    ip_update_length(ip, sizeof(struct udp_header) + len);

    return 0;
}

packet_t *udp_construct_packet(struct net_info *ni, 
                               eth_addr_t srceth, ip_addr_t srcip, port_t srcport,
                               ip_addr_t dstip, port_t dstport)
{
    int rc;
    packet_t *pkt;
    uint8_t *dsteth;

    /* FIXME: ouch */
    dstip = to_be_32(dstip);

    dsteth = arp_get_eth_addr(ni, dstip);

    if (dsteth == NULL)
        return NULL;

    pkt = ip_construct_packet_eth_full(srceth, dsteth, srcip, dstip);
    if (!pkt)
        return NULL;

    ip_set_proto(pkt->p_buf + pkt->pkt_ip_offset, IP_PROTO_UDP);

    rc = udp_add_header(pkt, srcport, dstport);
    if (rc)
        return NULL;

    pkt->pkt_proto_offset = pkt->p_ptr - pkt->p_buf;

    //udp_set_payload(pkt, udp_dummy_data, strlen(udp_dummy_data));

    return pkt;
}

packet_t *udp_new_packet(struct net_info *ni, port_t srcport, ip_addr_t dstip,
                            port_t dstport)
{
    return udp_construct_packet(ni, ni->ni_hw_mac, ni->ni_src_ip, srcport,
                                dstip, dstport);
}

int
do_udp_handle_packet(struct net_info *ni, packet_t *pkt, struct udp_header *udp)
{
    struct hash_elem *helem;
    struct udp_sock_priv *usp;
    size_t payload_len = to_be_16(udp->udp_len) - sizeof(struct udp_header);

    struct udp_sock_priv searcher = { .usp_srcport = to_be_16(udp->udp_dst_port) };

    /* find a struct udp_sock_priv to find the socket */
    helem = hash_find(&ni->ni_udp_sockets, &searcher.usp_helem);
    if (helem == NULL) {
        /* there is no socket connected on this end, drop this packet */

        /* TODO: send ICMP Destination Unreachable - Port Unreachable */
        return PACKET_DROP;
    }

    /* there is a socket listening on this side! */
    usp = hash_entry(helem, struct udp_sock_priv, usp_helem);

    /* is this connection blocked? */
    if (usp->usp_flags & USP_BLOCK)
        return PACKET_DROP;

    /* check if this socket has enough space */
    if (usp->usp_buffer_len + payload_len >= UDP_BUFFER_SIZE) {
        printk("WARNING: dropping packet because the dgram buffer is full\n");
        return PACKET_DROP;
    }

    /* write the payload of the packet to a dgram */
    struct udp_dgram *dgram = malloc(sizeof(*dgram));
    if (!dgram) {
        printk("WARNING: ENOMEM while allocating a dgram\n");
        return PACKET_DROP;
    }

    /* allocate space for the payload */
    void *dgram_buffer = malloc(payload_len);
    if (!dgram_buffer) {
        free(dgram);
        return PACKET_DROP;
    }

    /* copy the payload over */
    memcpy(dgram_buffer, pkt->p_ptr, payload_len);

    /* setup the dgram structure */
    dgram->udg_buffer = dgram_buffer;
    dgram->udg_len = payload_len;

    /* housekeeping */
    usp->usp_buffer_len += payload_len;

    /* off it goes */
    list_push_back(&usp->usp_dgrams, &dgram->udg_elem);

    //printk("WROTE %d\n", payload_len);

    return PACKET_HANDLED;
}

int
udp_handle_packet(struct net_info *ni, packet_t *pkt, struct udp_header *udp)
{
    net_printk(" ^ udp\n");
    pkt->pkt_proto_offset = (int)udp - (int)pkt->p_buf;
    pkt->p_ptr += sizeof(struct udp_header);
    /* try figuring out where the UDP packet is headed */
    if (udp->udp_dst_port == to_be_16(68) &&
            udp->udp_src_port == to_be_16(67)) {
        return dhcp_handle_packet(ni, pkt, udp);
    } else {
        return do_udp_handle_packet(ni, pkt, udp);
    }
    return PACKET_DROP;
}

int
do_udp_inet_sock_connect(struct socket *sock, struct sockaddr_in *addr, socklen_t len)
{
    struct udp_sock_priv *priv = sock->sock_priv;

    sock->sock_ni = route_find_ni_for_dst(addr->sin_addr);

    priv->usp_dstip = to_le_32(addr->sin_addr);
    priv->usp_dstport = addr->sin_port;
    priv->usp_srcport = net_allocate_port(SOCK_DGRAM);

    priv->usp_buffer_len = 0;
    list_init(&priv->usp_dgrams);

    hash_insert(&sock->sock_ni->ni_udp_sockets, &priv->usp_helem);

    net_printk("connected a UDP socket to %pI dstport %d srcport %d\n",
            priv->usp_dstip, priv->usp_dstport, priv->usp_srcport);

    return 0;
}

int
udp_sock_connect(struct socket *sock, struct sockaddr *addr, socklen_t len)
{
    sock->sock_priv = malloc(sizeof(struct udp_sock_priv));
    if (!sock->sock_priv)
        return -ENOMEM;

    if (sock->sock_domain == AF_INET)
        return do_udp_inet_sock_connect(sock, (struct sockaddr_in *) addr, len);

    printk("WARNING: trying to use UDP on not AF_INET!\n");
    return -EINVAL;
}

int
udp_sock_write(struct socket *sock, void *buf, size_t len)
{
    struct udp_sock_priv *priv = sock->sock_priv;
    struct net_device *ndev = NDEV_FROM_NI(sock->sock_ni);
    packet_t *pkt;

    if (priv == NULL)
        return -ENOTCONN;

    //printk("UDP socket write! from %pI:%d to %pI:%d\n", sock->sock_ni->ni_src_ip,
            //priv->usp_srcport, priv->usp_dstip, priv->usp_dstport);

    pkt = udp_new_packet(sock->sock_ni, priv->usp_srcport, priv->usp_dstip,
                            priv->usp_dstport);
    if (!pkt)
        return -ENOMEM;

    udp_set_payload(pkt, buf, len);

    ndev->send_packet(ndev, pkt);

    return 0;
}

struct udp_dgram *
udp_pop_dgram(struct udp_sock_priv *usp)
{
    struct list_elem *elem;
    struct udp_dgram *udg;

    /* wait for an element */
    while (list_empty(&usp->usp_dgrams))
        ;

    /* get the first datagram */
    elem = list_pop_front(&usp->usp_dgrams);

    /* we have a datagram! */
    udg = list_entry(elem, struct udp_dgram, udg_elem);

    /* housekeeping */
    usp->usp_buffer_len -= udg->udg_len;

    /* return */
    return udg;
}

int
udp_sock_read(struct socket *sock, void *buf, size_t len)
{
    struct udp_sock_priv *priv = sock->sock_priv;
    struct udp_dgram *udg = udp_pop_dgram(priv);
    size_t alen = len;

    /* find the maximum we can copy to userspace */
    if (udg->udg_len < alen)
        alen = udg->udg_len;

    /* copy */
    memcpy(buf, udg->udg_buffer, alen);

    /* destroy the datagram */
    free(udg->udg_buffer);
    free(udg);

    /* return the amount we've read */
    return alen;
}

int
udp_sock_destroy(struct socket *sock)
{
    struct udp_sock_priv *priv = sock->sock_priv;

    if (priv == NULL)
        return 0;

    /* don't accept anymore datagrams */
    priv->usp_flags |= USP_BLOCK;

    /* destroy the pending datagrams */
    while (!list_empty(&priv->usp_dgrams)) {
        struct list_elem *elem;
        struct udp_dgram *udg;

        elem = list_pop_front(&priv->usp_dgrams);
        udg = list_entry(elem, struct udp_dgram, udg_elem);
        free(udg->udg_buffer);
        free(udg);
    }

    net_free_port(SOCK_DGRAM, priv->usp_srcport);

    free(priv);

    return 0;
}

struct socket_ops udp_sock_ops = {
    .connect = udp_sock_connect,
    .write = udp_sock_write,
    .read = udp_sock_read,
    .destroy = udp_sock_destroy,
};
