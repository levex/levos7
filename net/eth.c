#include <levos/kernel.h>
#include <levos/packet.h>
#include <levos/eth.h>

#ifdef CONFIG_ETH_DEBUG
#define net_printk printk
#else
#define net_printk(...) ;
#endif

eth_addr_t
eth_broadcast_addr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

eth_addr_t
eth_null_addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void
printk_print_eth_addr(eth_addr_t print)
{
    uint8_t *ptr = (uint8_t *)print;
    printk("%x:%x:%x:%x:%x:%x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
            ptr[5], ptr[6]);
}

packet_t *
eth_construct_packet(eth_addr_t src, eth_addr_t dst, uint16_t eth_type)
{
    packet_t *pkt;

    /*printk("Constructing ethernet packet from %pE to %pE\n",
            src, dst);*/

    pkt = packet_allocate();
    if (!pkt)
        return NULL;

    if (packet_grow(pkt, sizeof(struct ethernet_header)))
        return NULL;

    struct ethernet_header *eth = (void *)pkt->p_buf;
    memcpy(eth->eth_src, src, sizeof(eth_addr_t));
    memcpy(eth->eth_dst, dst, sizeof(eth_addr_t));
    eth->eth_type = to_be_16(eth_type);

    return pkt;
}

void
eth_dump_packet(packet_t *pkt)
{
    if (pkt->p_buf == NULL)
        net_printk("eth packet is NULL\n");

    struct ethernet_header *eth = (void *)pkt->p_buf;
    net_printk("Ethernet frame: src %pE dst %pE\n", eth->eth_src, eth->eth_dst);
}

int
ethcmp(eth_addr_t _a, eth_addr_t _b)
{
    char *a = _a;
    char *b = _b;

    for (int i = 0; i < 6; i ++)
        if (a[i] != b[i])
            return 1;

    return 0;
}

int
eth_should_drop(struct net_info *ni, struct ethernet_header *eth)
{
    /* if it is addressed directly to us, then don't drop */
    if (ethcmp(ni->ni_hw_mac, eth->eth_dst) == 0)
        return 0;

    /* if it is sent to the broadcast address, then don't drop */
    if (ethcmp(eth_broadcast_addr, eth->eth_dst) == 0)
        return 0;

    /* drop  other frames for now */
    return 1;
}
