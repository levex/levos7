#include <levos/arp.h>
#include <levos/eth.h>
#include <levos/kernel.h>
#include <levos/ip.h>

#ifdef CONFIG_ARP_DEBUG
#define net_printk printk
#else
#define net_printk(...) ;
#endif

uint8_t
arp_get_plen(uint16_t ptype)
{
    if (ptype == ARP_PTYPE_IP)
        return ARP_PLEN_IP;

    return 0;
}

uint8_t
arp_get_hlen(uint16_t htype)
{
    if (htype == ARP_HTYPE_ETHERNET)
        return ARP_HLEN_ETHERNET;

    return 0;
}

uint32_t *
arp_get_pdst(struct arp_header *arp)
{
    uintptr_t offset = (uintptr_t) ((void *)&arp->arp_opcode + 2);
    offset += arp->arp_hlen;
    offset += arp->arp_plen;
    offset += arp->arp_hlen;

    return (uint32_t *) offset;
}

uint32_t *
arp_get_psrc(struct arp_header *arp)
{
    uintptr_t offset = (uintptr_t) ((void *)&arp->arp_opcode + 2);
    offset += arp->arp_hlen;

    return (uint32_t *) offset;
}

uint32_t *
arp_get_hsrc(struct arp_header *arp)
{
    uintptr_t offset = (uintptr_t) ((void *)&arp->arp_opcode + 2);

    return (uint32_t *) offset;
}

uint32_t *
arp_set_hdst(struct arp_header *arp, void *hdst)
{
    uintptr_t offset = (uintptr_t) ((void *)&arp->arp_opcode + 2);
    offset += arp->arp_hlen;
    offset += arp->arp_plen;
    memcpy((void *) offset, hdst, arp->arp_hlen);

    return (uint32_t *) offset;
}

int
arp_write_header(struct arp_header *buf, uint16_t ptype, uint8_t plen,
                 uint16_t htype, uint8_t hlen, uint16_t opcode,
                    void *hsrc, void *psrc,
                    void *hdst, void *pdst)
{
    uintptr_t offset = (uintptr_t) ((void *)&buf->arp_opcode + 2);

    //printk("%s: hsrc: %pE psrc: %pI hdst: %pE pdst: %pI\n",
            //__func__, hsrc, psrc, hdst, pdst);

    buf->arp_ptype = to_be_16(ptype);
    buf->arp_plen  = plen;
    buf->arp_htype = to_be_16(htype);
    buf->arp_hlen  = hlen;
    buf->arp_opcode = to_be_16(opcode);

    memcpy((void *) offset, hsrc, hlen);
    offset += hlen;

    memcpy((void *) offset, psrc, plen);
    offset += plen;

    memcpy((void *) offset, hdst, hlen);
    offset += hlen;

    memcpy((void *) offset, pdst, plen);

    return 0;
}

int
arp_add_header(packet_t *pkt, uint16_t htype, uint16_t ptype, uint8_t opcode,
                void *hsrc, void *psrc, void *hdst, void *pdst)
{
    uint8_t plen = arp_get_plen(ptype);
    uint8_t hlen = arp_get_hlen(htype);

    if (packet_grow(pkt, sizeof(struct arp_header) + 2 * (plen + hlen)))
        return -ENOMEM;

    /* we've grown the packet, add the header */
    arp_write_header(pkt->p_ptr, ptype, plen, htype, hlen, opcode,
                hsrc, psrc, hdst, pdst);

    return 0;
}

packet_t *
arp_construct_packet(uint16_t htype, uint16_t ptype, uint8_t opcode,
        void *hsrc, void *psrc, void *hdst, void *pdst)
{
    packet_t *pkt;
    int rc;

    pkt = eth_construct_packet(hsrc, eth_broadcast_addr, ETH_TYPE_ARP);
    if (!pkt)
        return NULL;

    rc = arp_add_header(pkt, htype, ptype, opcode, hsrc, psrc, hdst, pdst);
    if (rc)
        return NULL;

    return pkt;
}

packet_t *
arp_construct_request(uint16_t htype, uint16_t ptype, void *hsrc,
        void *psrc, void *hdst, void *pdst)
{
    return arp_construct_packet(htype, ptype, ARP_OPCODE_REQUEST,
            hsrc, psrc, hdst, pdst);
}

void
arp_send_reply_us(struct net_info *ni, struct arp_header *arp)
{
    struct arp_header *mod;
    struct ethernet_header *eth;
    struct net_device *ndev = NDEV_FROM_NI(ni);

    packet_t *pkt = arp_construct_request_eth_ip(ni->ni_hw_mac, ni->ni_src_ip, 
            to_be_32(*arp_get_psrc(arp)));

    eth = pkt->p_buf;
    mod = pkt->p_ptr;
    memcpy(&eth->eth_dst, arp_get_hsrc(arp), 6);
    mod->arp_opcode = to_be_16(ARP_OPCODE_REPLY);
    arp_set_hdst(mod, arp_get_hsrc(arp));

    ndev->send_packet(ndev, pkt);
}

packet_t *
arp_construct_request_eth_ip(void *hsrc, ip_addr_t psrc, ip_addr_t pdst)
{
    be_uint32_t bpsrc = to_be_32(psrc);
    be_uint32_t bpdst = to_be_32(pdst);

    return arp_construct_request(ARP_HTYPE_ETHERNET, ARP_PTYPE_IP,
            hsrc, &bpsrc, eth_null_addr, &bpdst);
}

int
arp_try_eth_ip_request(struct net_info *ni, packet_t *pkt, struct arp_header *arp)
{
    ip_addr_t target_ip = *arp_get_pdst(arp);
    ip_addr_t usip = to_be_32(ni->ni_src_ip);

    net_printk("arp: request of %pI us: %pI\n", target_ip, usip);

    if (ipcmp(target_ip, usip) == 0) {
        net_printk("arp: we received a request for our ethernet address!\n");
        arp_send_reply_us(ni, arp);
    }
}

int
arp_handle_request(struct net_info *ni, packet_t *pkt, struct arp_header *arp)
{
    uintptr_t offset = (uintptr_t) ((void *)&arp->arp_opcode + 2);
    void *base = arp;
    int hlen = arp->arp_hlen, plen = arp->arp_plen;

    /* if our ARP engine is offline, drop the packet */
    if (!ni->ni_arp_kick)
        return PACKET_DROP;

    /* eth to IP translation */
    if (hlen == 6 && plen == 4)
        return arp_try_eth_ip_request(ni, pkt, arp);

    net_printk("ARP request: hsrc %pE psrc %pI hdst %pE pdst %pI\n",
            offset, offset + hlen, offset + hlen + plen,
            offset + hlen + plen + hlen);

    return PACKET_DROP;
}

int
arp_handle_reply(struct net_info *ni, packet_t *pkt, struct arp_header *arp)
{
    uintptr_t offset = (uintptr_t) ((void *)&arp->arp_opcode + 2);
    void *base = arp;
    int hlen = arp->arp_hlen, plen = arp->arp_plen;
    net_printk("ARP reply: hsrc %pE psrc %pI hdst %pE pdst %pI\n",
            offset, offset + hlen, offset + hlen + plen,
            offset + hlen + plen + hlen);

    arp_cache_insert(*((ip_addr_t *) (offset + hlen)), (uint8_t *) offset);

    return PACKET_DROP;
}

int
arp_handle_packet(struct net_info *ni, packet_t *pkt, struct arp_header *arp)
{
    if (to_le_16(arp->arp_opcode) == ARP_OPCODE_REQUEST) {
        net_printk(" ^   request\n");
        return arp_handle_request(ni, pkt, arp);
    } else if (to_le_16(arp->arp_opcode) == ARP_OPCODE_REPLY) {
        net_printk(" ^   reply\n");
        return arp_handle_reply(ni, pkt, arp);
    } else { 
        net_printk(" ^   invalid arp opcode %d\n", to_le_16(arp->arp_opcode));
        return PACKET_DROP;
    }
}
