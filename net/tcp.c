#include <levos/kernel.h>
#include <levos/eth.h>
#include <levos/tcp.h>
#include <levos/ip.h>
#include <levos/arp.h>
#include <levos/work.h>

/* TODO list:
 * 1) segment reconstruction
 * 2) parse MSS from SYNACK options
 * 3) get rid of e1000_device and fold into net_device       TICK
 * 4) proper state management
 * 5) robust RST send off
 */

void
tcp_write_header(struct tcp_header *tcp, port_t srcport, port_t dstport)
{
    memset(tcp, 0, sizeof(*tcp));
    tcp->tcp_src_port = to_be_16(srcport);
    tcp->tcp_dst_port = to_be_16(dstport);
    tcp_set_doff(tcp, 5);
    net_printk("Doff: %d\n", tcp_get_doff(tcp));
    tcp->tcp_wsize = 0xffff;

    /* the other fields as well as the checksum are set later */
}

int
tcp_set_payload(packet_t *pkt, void *data, size_t sz)
{
    int rc;

    /* grow the packet to ensure enough space */
    rc = packet_grow(pkt, sz);
    if (rc)
        return -ENOMEM;

    /* copy the payload */
    memcpy(pkt->p_ptr, data, sz);

    return 0;
}

uint16_t
tcp_calculate_checksum(struct ip_base_header *ip, struct tcp_header *tcp, size_t payload_sz)
{
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *) tcp;
    int i;

    for (i = 0; i < (payload_sz + sizeof(*tcp)) / sizeof(uint16_t); i ++)
        sum += to_be_16(ptr[i]);

    if ((payload_sz + sizeof(*tcp)) % 2) {
        /* fetch the last byte */
        uint8_t *buf = (uint8_t *) tcp;
        int loc = sizeof(*tcp) + payload_sz - 1;
        uint16_t s = buf[loc] << 8;
        sum += s;
    }

    /* add IP pseudo header */
    ptr = (uint16_t *) &ip->ip_srcaddr;
    sum += to_be_16(ptr[0]);
    sum += to_be_16(ptr[1]);

    ptr = (uint16_t *) &ip->ip_dstaddr;
    sum += to_be_16(ptr[0]);
    sum += to_be_16(ptr[1]);

    sum += (uint16_t) IP_PROTO_TCP;

    sum += sizeof(*tcp) + payload_sz;

    while (sum > 0x0000ffff) {
        uint16_t top = (sum & 0xffff0000) >> 16;
        sum &= 0x0000ffff;
        sum += top;
    }

    sum = ~sum;

    return to_be_16(sum);
}

void
tcp_finalize_packet(struct tcp_header *tcp, size_t payload_sz)
{
    struct ip_base_header *ip = (void *)tcp - sizeof(struct ip_base_header);
    /* calculate the checksum */
    tcp->tcp_chksum = 0;
    tcp->tcp_chksum = tcp_calculate_checksum(ip, tcp, payload_sz);

    /* update IP */
    ip_update_length(ip, sizeof(struct tcp_header) + payload_sz);
}

int
tcp_add_header(packet_t *pkt, port_t srcport, port_t dstport)
{
    if (packet_grow(pkt, sizeof(struct tcp_header)))
        return -ENOMEM;

    pkt->pkt_proto_offset = pkt->p_ptr - pkt->p_buf;

    tcp_write_header(pkt->p_ptr, srcport, dstport);

    return 0;
}

packet_t *
tcp_construct_packet(struct net_info *ni, eth_addr_t hw_mac, ip_addr_t _srcip, port_t srcport,
        ip_addr_t _dstip, port_t dstport)
{
    int rc;
    uint8_t *dsteth;
    be_ip_addr_t srcip = to_be_32(_srcip);
    be_ip_addr_t dstip = to_be_32(_dstip);

    net_printk("%s: constructing packet to %pI from %pI\n", __func__, dstip, srcip);

    dsteth = arp_get_eth_addr(ni, _dstip);
    if (dsteth == NULL) {
        net_printk("%s: failed to acquire the eth address\n", __func__);
        return NULL;
    }

    net_printk("%s: acquired eth address: %pE\n", __func__, dsteth);

    packet_t *pkt = ip_construct_packet_eth_full(hw_mac, dsteth, _srcip, _dstip);
    if (!pkt)
        return NULL;

    ip_set_proto(pkt->p_buf + pkt->pkt_ip_offset, IP_PROTO_TCP);

    rc = tcp_add_header(pkt, srcport, dstport);
    if (rc)
        return NULL;

    return pkt;
}

packet_t *
tcp_new_packet(struct net_info *ni, port_t srcport, ip_addr_t dstip,
                            port_t dstport)
{
    return tcp_construct_packet(ni, ni->ni_hw_mac, ni->ni_src_ip, srcport,
                                dstip, dstport);
}

unsigned tcp_hash_tcp_info(const struct hash_elem *e, void *aux)
{
    struct tcp_info *ti = hash_entry(e, struct tcp_info, ti_helem);

    return hash_int(ti->ti_src_port);
}

bool tcp_less_tcp_info(const struct hash_elem *a,
                     const struct hash_elem *b,
                     void *aux)
{
    struct tcp_info *ta = hash_entry(a, struct tcp_info, ti_helem);
    struct tcp_info *tb = hash_entry(b, struct tcp_info, ti_helem);

    return ta->ti_src_port < ta->ti_src_port;
}

struct tcp_info *
__tcp_find_info(struct net_info *ni, port_t srcport)
{
    struct tcp_info *ret = NULL;
    struct hash_elem *elem;

    struct tcp_info cmp = {
        .ti_src_port = srcport,
    };

    elem = hash_find(&ni->ni_tcp_infos, &cmp.ti_helem);
    if (elem)
        ret = hash_entry(elem, struct tcp_info, ti_helem);

    return ret;
}

struct tcp_info *
tcp_find_info(struct net_info *ni, port_t srcport)
{
    struct tcp_info *ret;

    spin_lock(&ni->ni_tcp_infos_lock);
    ret = __tcp_find_info(ni, srcport);
    spin_unlock(&ni->ni_tcp_infos_lock);

    return ret;
}

void
tcp_do_fail(struct net_info *ni, struct tcp_info *ti, int errno)
{
    ti->ti_tcp_state = TI_STATE_FAILED;
    ti->ti_fail_code = errno;
}

void
tcp_notify_retransmit_failed(struct net_info *ni, packet_t *pkt)
{
    struct tcp_info *ti;

    /* first extract the srcport */
    struct tcp_header *tcp = pkt->p_buf + pkt->pkt_proto_offset;

    /* find the corresponding tcp_info */
    ti = tcp_find_info(ni, tcp->tcp_src_port);
    panic_ifnot(ti != NULL);
    /* ^ this assert is valid, since we can't fault on a retransmission
     * that lacks a tcp_info
     */

    ti->ti_fail_code = -ETIMEDOUT;
    ti->ti_tcp_state = TI_STATE_FAILED;
    return;
}

void
tcp_send_packet(struct net_info *ni, struct tcp_info *ti, packet_t *pkt, size_t len)
{
    struct net_device *ndev = NDEV_FROM_NI(ni);
    struct tcp_header *tcp = pkt->p_buf + pkt->pkt_proto_offset;
    struct work *rt;

    tcp->tcp_seq = to_be_32(ti->ti_next_seq);

    /* if FIN or SYN is set then increase, otherwise leave as is */
    ti->ti_next_seq += len;

    /* FIXME: other _finalize_packet calls should be removed */
    tcp_finalize_packet(tcp, len);

    rt = packet_schedule_retransmission(ni, pkt, 10, 50,
            tcp_notify_retransmit_failed);
    ti->ti_retransmit_work = rt;

    ndev->send_packet(ndev, pkt);
}

int
tcp_conn_send(struct net_info *ni, struct tcp_info *ti, void *data, size_t len)
{
    struct net_device *ndev = NDEV_FROM_NI(ni);
    packet_t *pkt;
    int rc;
    struct tcp_header *tcp;

    /* FIXME s:
     * is ENOTCONN the right errno to return here?
     * can two processes send data to the same TI and NI ?
     */

    if (ti->ti_tcp_state != TI_STATE_ESTABLISHED)
        return -ENOTCONN;

    pkt = tcp_new_packet(ni, ti->ti_src_port, ti->ti_dstip, ti->ti_dst_port);
    if (!pkt)
        return -ENOMEM;

    /* set the payload */
    rc = tcp_set_payload(pkt, data, len);
    if (rc)
        return rc;

    /* get a handle to the TCP header */
    tcp = pkt->p_buf + pkt->pkt_proto_offset;

    /* we are pushing data */
    tcp_set_psh(tcp, 1);
    tcp_set_ack(tcp, 1);
    tcp->tcp_ack = to_be_32(ti->ti_next_ack);

    tcp_finalize_packet(tcp, len);

    /* TODO */

    /* off it goes... */
    //e1000_send_packet(edev, pkt);
    tcp_send_packet(ni, ti, pkt, len);

    return 0;
}

int
tcp_conn_start(struct net_info *ni, port_t srcport, ip_addr_t dstip,
        port_t dstport)
{
    packet_t *pkt;
    struct tcp_info *ti = NULL;
    int rc;

    /* allocate the tcp_info structure */
    ti = malloc(sizeof(*ti));
    if (!ti) {
        rc = -ENOMEM;
        goto fail_nolock;
    }

    /* lock since we are manipulating */
    spin_lock(&ni->ni_tcp_infos_lock);

    /* check if srcport is used */
    if (__tcp_find_info(ni, srcport) != NULL) {
        rc = -EADDRINUSE;
        goto fail;
    }

    /* setup the tcp_info structure */
    ti->ti_src_port = srcport;
    ti->ti_dst_port = dstport;
    ti->ti_next_ack = 0;
    ti->ti_next_seq = 0;
    ti->ti_tcp_state = TI_STATE_UNDEFINED;
    ti->ti_dstip = dstip;

    /* insert the tcp_info */
    hash_insert(&ni->ni_tcp_infos, &ti->ti_helem);

    /* drop the lock, we are finished with the hashtable */
    spin_unlock(&ni->ni_tcp_infos_lock);

    /* create a packet */
    pkt = tcp_new_packet(ni, srcport, dstip, dstport);
    if (!pkt) {
        rc = -ENOMEM;
        goto fail_nolock;
    }

    /* set SYN */
    tcp_packet_set_syn(pkt, 1);

    /* finalize packet */
    tcp_finalize_packet(pkt->p_buf + pkt->pkt_proto_offset, 0);

    ti->ti_tcp_state = TI_STATE_SYN_SENT;

    /* off we go */
    //e1000_send_packet(edev, pkt);
    tcp_send_packet(ni, ti, pkt, 0);

    struct work *rt;
    rt = packet_schedule_retransmission(ni, pkt, 5, 1000, tcp_notify_retransmit_failed);
    ti->ti_retransmit_work = rt;

    while (ti->ti_tcp_state != TI_STATE_ESTABLISHED &&
            ti->ti_tcp_state != TI_STATE_FAILED)
        ;

    if (ti->ti_tcp_state == TI_STATE_FAILED)
        return ti->ti_fail_code;

    return 0;

fail:
    spin_unlock(&ni->ni_tcp_infos_lock);
fail_nolock:
    free(ti);
    return rc;
}

int
tcp_conn_close(struct net_info *ni, struct tcp_info *ti)
{
    packet_t *pkt;

    /* send a FIN */
    pkt = tcp_new_packet(ni, ti->ti_src_port, ti->ti_dstip, ti->ti_dst_port);
    if (!pkt)
        return -ENOMEM;

    struct tcp_header *tcp = pkt->p_buf + pkt->pkt_proto_offset;

    tcp_set_fin(tcp, 1);
    //ti->ti_next_seq ++;

    tcp_set_ack(tcp, 1);
    tcp->tcp_ack = to_be_32(ti->ti_next_ack);

    tcp_finalize_packet(tcp, 0);

    ti->ti_tcp_state = TI_STATE_FIN_SENT;

    //e1000_send_packet(edev, pkt);
    tcp_send_packet(ni, ti, pkt, 0);

    while (ti->ti_tcp_state != TI_STATE_CLOSED
            && ti->ti_tcp_state != TI_STATE_FAILED) {
        if (ti->ti_tcp_state == TI_STATE_FAILED) {
            net_printk("Forcefully closing connection\n");
            ti->ti_tcp_state = TI_STATE_CLOSED;
            /* FIXME is this correct errno */
            return -ECONNRESET;
        }
    }

    return 0;
}

int
tcp_handle_synack(struct net_info *ni, packet_t *pkt, struct tcp_info *ti,
            struct tcp_header *tcp)
{
    packet_t *packet;
    net_printk("   ^ synack\n");

    /* if a synack is sent in the wrong state, we don't handle that yet */
    if (ti->ti_tcp_state != TI_STATE_SYN_SENT)
        return PACKET_DROP;

    ti->ti_tcp_state = TI_STATE_SYNACK;

    /* continue establishing the connection by ACKing */
    packet = tcp_new_packet(ni, ti->ti_src_port, ti->ti_dstip, ti->ti_dst_port);
    if (!packet)
        return PACKET_DROP;

    struct tcp_header *ntcp = packet->p_buf + packet->pkt_proto_offset;

    tcp_packet_set_ack(packet, 1);
    ntcp->tcp_seq = to_be_32(to_le_32(tcp->tcp_ack));
    ntcp->tcp_ack = to_be_32(to_le_32(tcp->tcp_seq) + 1);
    tcp_finalize_packet(ntcp, 0);

    //ti->ti_next_seq = to_le_32(tcp->tcp_ack);
    ti->ti_next_ack = to_le_32(tcp->tcp_seq) + 1;

    /* send the ACK packet */
    tcp_send_packet(ni, ti, packet, 0);

    net_printk("Sent an ACK\n");

    ti->ti_tcp_state = TI_STATE_ESTABLISHED;

    return PACKET_HANDLED;
}

void
tcp_cancel_retransmission(struct tcp_info *ti)
{
    if (ti->ti_retransmit_work)
        work_cancel(ti->ti_retransmit_work);
}

void
tcp_handle_ack(struct net_info *ni, packet_t *pkt, struct tcp_info *ti, struct tcp_header *tcp)
{
    net_printk("   ^ ack\n");

    /* we received an ACK, so cancel the retransmission,
     * FIXME is this really correct? what if this ACK is for an old
     * packet?
     */
    tcp_cancel_retransmission(ti);

    if (ti->ti_tcp_state == TI_STATE_FINACK_SENT) {
        ti->ti_tcp_state == TI_STATE_CLOSED;
        net_printk("tcp connection closed!\n");
        return;
    }

    //ti->ti_next_ack = tcp->tcp_seq + 1;
    //ti->ti_next_seq = tcp->tcp_ack;
}

int
tcp_handle_fin(struct net_info *ni, packet_t *pkt, struct tcp_info *ti,
        struct tcp_header *tcp)
{
    packet_t *packet;

    net_printk(" ^^ FIN\n");

    if (ti->ti_tcp_state != TI_STATE_FIN_SENT)
        ti->ti_tcp_state = TI_STATE_FIN_RECV;

    /* TODO check if we have data to send in a buffer */

    /* ack this packet */
    //__tcp_ack_packet(ni, pkt, ti, tcp, 0);

    /* otherwise, send [FIN, ACK] */
    packet = tcp_new_packet(ni, ti->ti_src_port, ti->ti_dstip, ti->ti_dst_port);
    if (!packet)
        return PACKET_DROP;

    struct tcp_header *ntcp = packet->p_buf + packet->pkt_proto_offset;

    ntcp->tcp_wsize = 0;
    tcp_set_fin(ntcp, 1);

    if (ti->ti_tcp_state == TI_STATE_FIN_RECV) {
        tcp_set_ack(ntcp, 1);
        ntcp->tcp_ack = to_be_32(to_le_32(tcp->tcp_seq) + 1);
        ntcp->tcp_seq = to_be_32(to_le_32(tcp->tcp_ack));
    }

    tcp_finalize_packet(ntcp, 0);

    if (ti->ti_tcp_state == TI_STATE_FIN_RECV)
        ti->ti_tcp_state = TI_STATE_FINACK_SENT;
    else
        ti->ti_tcp_state = TI_STATE_CLOSED;

    tcp_send_packet(ni, ti, packet, 0);

    return PACKET_HANDLED;
}

int
tcp_reset_connection(struct net_info *ni, struct tcp_info *ti)
{
    packet_t *pkt;

    net_printk("TCP: fatal error occured, resetting the connection src %d dst %d\n",
            ti->ti_src_port, ti->ti_dst_port);

    pkt = tcp_new_packet(ni, ti->ti_src_port, ti->ti_dstip, ti->ti_dst_port);
    if (!pkt)
        return PACKET_DROP;

    struct tcp_header *tcp = pkt->p_buf + pkt->pkt_proto_offset;
    tcp_set_rst(tcp, 1);
    tcp_finalize_packet(tcp, 0);

    tcp_send_packet(ni, ti, pkt, 0);

    return PACKET_DROP;
}

uint32_t
tcp_get_payload_size(packet_t *pkt, struct tcp_header *tcp)
{
    uint32_t sz = pkt->p_len;

    sz -= pkt->pkt_proto_offset;

    sz -= tcp_get_doff(tcp) * sizeof(uint32_t);

    return sz;
}

void *
tcp_get_payload(struct tcp_header *tcp)
{
    return (void *)tcp + (tcp_get_doff(tcp) * sizeof(uint32_t));
}

void
__tcp_ack_packet(struct net_info *ni, packet_t *pkt, struct tcp_info *ti, 
        struct tcp_header *tcp, uint32_t psize)
{
    packet_t *packet;

    packet = tcp_new_packet(ni, ti->ti_src_port, ti->ti_dstip, ti->ti_dst_port);
    if (!packet)
        return;

    struct tcp_header *ntcp = packet->p_buf + packet->pkt_proto_offset;

    tcp_set_ack(ntcp, 1);

    ntcp->tcp_ack = to_be_32(to_le_32(tcp->tcp_seq) + psize);
    //ntcp->tcp_seq = to_be_32(to_le_32(tcp->tcp_ack) + 1);

    tcp_finalize_packet(ntcp, 0);

    tcp_send_packet(ni, ti, packet, 0);
    return;
}

void
tcp_ack_packet(struct net_info *ni, packet_t *pkt, struct tcp_info *ti, 
        struct tcp_header *tcp)
{
    __tcp_ack_packet(ni, pkt, ti, tcp, tcp_get_payload_size(pkt, tcp));
}

/* FIXME: track the buffer somehow and appropriately do reordering */
int
tcp_handle_push(struct net_info *ni, packet_t *pkt, struct tcp_info *ti,
        struct tcp_header *tcp)
{

    net_printk(" ^^ PSH\n");

    uint32_t payload_size = tcp_get_payload_size(pkt, tcp);
    void *tcp_payload = tcp_get_payload(tcp);

    net_printk("Got payload of size %d string: %s\n", payload_size, tcp_payload);

    tcp_ack_packet(ni, pkt, ti, tcp);

    return PACKET_DROP;
}

int
tcp_handle_packet(struct net_info *ni, packet_t *pkt, struct tcp_header *tcp)
{
    struct tcp_info *ti;

    net_printk(" ^^ tcp\n");

    /* save the offset */
    pkt->pkt_proto_offset = pkt->p_ptr - pkt->p_buf;

    net_printk("TCP port: %d\n", tcp->tcp_dst_port);

    /* try to find a tcp_info for the target port */
    ti = tcp_find_info(ni, tcp->tcp_dst_port);
    if (ti == NULL) {
        net_printk("   ^no tcp_info for this packet, dropped\n");
        return PACKET_DROP;
    }

    /* do we increase seq? */
    if (tcp_is_set_syn(tcp) || tcp_is_set_fin(tcp))
        ti->ti_next_seq ++;

    /* handle ACK packets quickly */
    if (tcp_is_set_ack(tcp)) {
        tcp_handle_ack(ni, pkt, ti, tcp);
        if (tcp_is_set_syn(tcp))
            return tcp_handle_synack(ni, pkt, ti, tcp);
    }

    if (tcp_is_set_rst(tcp)) {
        if (ti->ti_tcp_state == TI_STATE_SYN_SENT)
            tcp_do_fail(ni, ti, ECONNREFUSED);
        else
            tcp_do_fail(ni, ti, ECONNRESET);
        return PACKET_HANDLED;
    }

    if (tcp_is_set_psh(tcp))
        return tcp_handle_push(ni, pkt, ti, tcp);

    if (tcp_is_set_fin(tcp))
        return tcp_handle_fin(ni, pkt, ti, tcp);

    net_printk("   ^ mere ACK packet\n");

    return PACKET_DROP;
}

void
test_tcp(struct net_info *ni)
{
    int rc;
    struct tcp_info *ti;
    char *testpayload =  "Hello internet from LevOS 7\n";
    char *testpayload2 = "These were sent from LevOS!!\n";
    struct net_device *ndev = NDEV_FROM_NI(ni);

    /* bring it up */
    ndev->up(ndev);

    net_printk("test_tcp: doing a quick test\n");
    rc = tcp_conn_start(ni, 1337, IP(192, 168, 0, 137), 7548);
    if (rc) {
        net_printk("tcp start connection from 1337 to 7548 failed with %s\n",
                errno_to_string(rc));
        return;
    }

    ti = tcp_find_info(ni, 1337);
    panic_ifnot(ti != NULL);

    net_printk("CONNECTION ESTABLISHED\n");
    sleep(10);
    rc = tcp_conn_send(ni, ti, testpayload, strlen(testpayload));
    if (rc) {
        net_printk("tcp send 1 failed with %s\n", errno_to_string(rc));
    }
    rc = tcp_conn_send(ni, ti, testpayload2, strlen(testpayload2));
    if (rc) {
        net_printk("tcp send 2 failed with %s\n", errno_to_string(rc));
    }
    net_printk("DATA SUCCESSFULLY SENT\n");
    rc = tcp_conn_close(ni, ti);
    if (rc) {
        net_printk("tcp failed to close %s\n", errno_to_string(rc));
    }
}
