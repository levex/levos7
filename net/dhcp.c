#include <levos/kernel.h>
#include <levos/work.h>
#include <levos/udp.h>
#include <levos/ip.h>
#include <levos/dhcp.h>
#include <levos/tcp.h>
#include <levos/e1000.h> /* FIXME: make it net_device eventually */

#define DHCP_LEVOS_XID 0x13377331

void
dhcp_write_discover_header(struct dhcp_packet *dhcp, struct net_info *ni)
{
    memset(dhcp, 0, sizeof(*dhcp));

    dhcp->dhcp_op = DHCP_OP_DISCOVER;
    dhcp->dhcp_htype = DHCP_HTYPE_ETH;
    dhcp->dhcp_hlen = DHCP_HLEN_ETH;
    dhcp->dhcp_hops = 0;
    dhcp->dhcp_xid = DHCP_LEVOS_XID;
    dhcp->dhcp_secs = 0x0000;
    dhcp->dhcp_flags = to_be_16(0x8000);
    dhcp->dhcp_ciaddr = dhcp->dhcp_yiaddr = dhcp->dhcp_siaddr = 0;
    dhcp->dhcp_giaddr = 0;
    memcpy(dhcp->dhcp_chaddr, ni->ni_hw_mac, 6);
    dhcp->dhcp_cookie = to_be_32(DHCP_MAGIC_COOKIE);
}

int
dhcp_write_option_ptr(struct dhcp_packet *dhcp, uintptr_t optoff,
            uint8_t optcode, uint8_t optsz, uint8_t *optdata)
{
    uint8_t *ptr = (uint8_t *) dhcp;
    int i;

    ptr += sizeof(struct dhcp_packet);
    ptr += optoff;

    if (optcode == 255) {
        *ptr = 255;
        return 1;
    }

    *ptr++ = optcode;
    *ptr++ = optsz;
    for (i = 0; i < optsz; i ++)
        ptr[i] = optdata[i];

    return optsz + 2;
}

int
dhcp_write_option(struct dhcp_packet *dhcp, uintptr_t optoff,
        uint8_t opcode, uint8_t opsz, uint32_t val)
{
    return dhcp_write_option_ptr(dhcp, optoff, opcode, opsz, (uint8_t *) &val);
}

void
dhcp_write_request_header(struct dhcp_packet *dhcp, struct net_info *ni,
        ip_addr_t our_ip, ip_addr_t server_ip)
{
    memset(dhcp, 0, sizeof(*dhcp));

    dhcp->dhcp_op = DHCP_OP_REQUEST;
    dhcp->dhcp_htype = DHCP_HTYPE_ETH;
    dhcp->dhcp_hlen = DHCP_HLEN_ETH;
    dhcp->dhcp_hops = 0;
    dhcp->dhcp_xid = DHCP_LEVOS_XID;
    dhcp->dhcp_secs = 0x0000;
    dhcp->dhcp_flags = to_be_16(0x8000);
    dhcp->dhcp_ciaddr = dhcp->dhcp_yiaddr = dhcp->dhcp_giaddr = 0;
    dhcp->dhcp_siaddr = server_ip;
    memcpy(dhcp->dhcp_chaddr, ni->ni_hw_mac, 6);
    dhcp->dhcp_cookie = to_be_32(DHCP_MAGIC_COOKIE);
}

packet_t *
dhcp_create_discover_packet(struct net_info *ni)
{
    packet_t *pkt;
    int rc;
    int optoff = 0;

    pkt = udp_new_packet(ni, DHCP_SOURCE_PORT, IP(255, 255, 255, 255), DHCP_DEST_PORT);
    if (!pkt)
        return NULL;

    struct dhcp_packet *dhcp = malloc(sizeof(struct dhcp_packet) + 128);
    dhcp_write_discover_header(dhcp, ni);
    optoff += dhcp_write_option(dhcp, optoff,  53, 1, 1);
    optoff += dhcp_write_option(dhcp, optoff, 255, 0, 0);

    udp_set_payload(pkt, dhcp, sizeof(*dhcp) + optoff);
    free(dhcp);

    return pkt;
}

packet_t *
dhcp_create_request_packet(struct net_info *ni, ip_addr_t our_ip,
            ip_addr_t server_ip)
{
    packet_t *pkt;
    int rc;
    int optoff = 0;

    pkt = udp_new_packet(ni, DHCP_SOURCE_PORT, IP(255, 255, 255, 255), DHCP_DEST_PORT);
    if (!pkt)
        return NULL;

    struct dhcp_packet *dhcp = malloc(sizeof(struct dhcp_packet) + 128);
    dhcp_write_request_header(dhcp, ni, our_ip, server_ip);
    optoff  += dhcp_write_option(dhcp, optoff, 53,  1, 3);
    optoff  += dhcp_write_option(dhcp, optoff, 50,  4, our_ip);
    optoff  += dhcp_write_option(dhcp, optoff, 54,  4, server_ip);
    optoff  += dhcp_write_option(dhcp, optoff, 255, 0, 0);

    udp_set_payload(pkt, dhcp, sizeof(*dhcp) + optoff);
    free(dhcp);

    return pkt;
    
}

int
dhcp_handle_offer(struct net_info *ni, packet_t *pkt,
        struct dhcp_packet *dhcp, size_t len)
{
    struct net_device *ndev = NDEV_FROM_NI(ni);
    ip_addr_t our_ip;
    ip_addr_t server_ip;

    net_printk("    ^ offer\n");
    if (ni->ni_dhcp_state != NI_DHCP_STATE_DISCOVER)
        return PACKET_DROP; /* we already have an IP */

    net_printk("    ^ offer not dropped\n");

    ni->ni_dhcp_state = NI_DHCP_STATE_OFFER;

    our_ip = dhcp->dhcp_yiaddr;
    server_ip = dhcp->dhcp_siaddr;

    ni->ni_src_ip = to_le_32(our_ip);

    /* fetch our IP address */
    net_printk("dhcp: got IP address offer %pI from %pI\n", our_ip, server_ip);

    /* kick the arp engine */
    ni->ni_arp_kick = 1;
    net_printk("^ kicked ARP\n");

    /* send a request packet */
    pkt = dhcp_create_request_packet(ni, our_ip, server_ip);
    ndev->send_packet(ndev, pkt);

    return PACKET_HANDLED;
}

int
dhcp_handle_ack(struct net_info *ni, packet_t *pkt,
        struct dhcp_packet *dhcp, size_t len)
{
    struct net_device *ndev = NDEV_FROM_NI(ni);
    net_printk("^ DHCP ACK\n");

    if (ni->ni_dhcp_state != NI_DHCP_STATE_OFFER)
        return PACKET_DROP;

    ni->ni_dhcp_state = NI_DHCP_STATE_VALID;
 
    net_printk("dhcp: acquired network state, as IP %pI\n", ni->ni_src_ip);

    return PACKET_HANDLED;
}

int
dhcp_do_handle_packet(struct net_info *ni, packet_t *pkt,
                        struct dhcp_packet *dhcp, size_t len)
{
    if (dhcp->dhcp_xid == DHCP_LEVOS_XID) {
        net_printk("   ^ addressed to us!\n");
        if (ni->ni_dhcp_state == NI_DHCP_STATE_DISCOVER && dhcp->dhcp_op == DHCP_OP_OFFER)
            return dhcp_handle_offer(ni, pkt, dhcp, len);
        else if (dhcp->dhcp_op == DHCP_OP_ACK)
            return dhcp_handle_ack(ni, pkt, dhcp, len);
        else
            net_printk("  ^ invalid DHCP op %d\n", dhcp->dhcp_op);
    } else
        net_printk("   ^ not addressed to us :(\n");
    return PACKET_DROP;
}

int
dhcp_handle_packet(struct net_info *ni, packet_t *pkt, struct udp_header *udp)
{

    size_t len = udp->udp_len;

    return dhcp_do_handle_packet(ni, pkt, pkt->p_ptr, len);
}

void
send_dhcp_disco(struct net_device *ndev)
{
    struct net_info *ni = &ndev->ndev_ni;

    if (ni->ni_dhcp_state != NI_DHCP_STATE_DISCOVER)
        return;

    if (ni->ni_dhcp_tries == 0) {
        net_printk("dhcp: failed to retrieve network configuration\n");
        ni->ni_dhcp_state = NI_DHCP_STATE_VALID;
        ni->ni_src_ip = IP(169, 254, 13, 37);
        ni->ni_arp_kick = 1;
        net_printk("staticip: using Link Local addressing as %pI\n", ni->ni_src_ip);

        return;
    }

    net_printk("dhcp: %strying dhcp discovery... %d tr%s left...\n",
            ni->ni_dhcp_tries == 2 ? "" : "re",
            -- ni->ni_dhcp_tries,
            ni->ni_dhcp_tries <= 2 ? "y" : "ies");

    packet_t *packet;
    packet = dhcp_create_discover_packet(ni);
    ndev->send_packet(ndev, packet);

    struct work *this = work_create((void (*)(void *))send_dhcp_disco, ndev);
    schedule_work_delay(this, 1000);
}

void
dhcp_start_discovery(struct net_device *ndev, struct net_info *ni)
{
    ni->ni_dhcp_state = NI_DHCP_STATE_DISCOVER;
    ni->ni_dhcp_tries = 3;

    struct work *work = work_create((void (*)(void *))send_dhcp_disco, ndev);
    schedule_work_delay(work, 200);
}

void
do_dhcp(struct net_device *ndev)
{
    struct net_info *ni = &ndev->ndev_ni;

    dhcp_start_discovery(ndev, ni);

    while(ni->ni_dhcp_state != NI_DHCP_STATE_VALID
            && ni->ni_dhcp_state != NI_DHCP_STATE_OFFER)
        barrier();
}
