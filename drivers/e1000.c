#include <levos/e1000.h>
#include <levos/packet.h>
#include <levos/pci.h>
#include <levos/kernel.h>
#include <levos/page.h>
#include <levos/eth.h>
#include <levos/arp.h>
#include <levos/tcp.h>
#include <levos/udp.h>
#include <levos/dhcp.h>
#include <levos/ip.h>
#include <levos/palloc.h>
#include <levos/intr.h>
#include <levos/socket.h>

uint8_t test_packet[] = 
{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* eth dest (broadcast) */
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56, /* eth source */
    0x08, 0x06, /* eth type */
    0x00, 0x01, /* ARP htype */
    0x08, 0x00, /* ARP ptype */
    0x06, /* ARP hlen */
    0x04, /* ARP plen */
    0x00, 0x01, /* ARP opcode: ARP_REQUEST */
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56, /* ARP hsrc */
    169, 254, 13, 37, /* ARP psrc */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ARP hdst */
    192, 168, 0, 137, /* ARP pdst */
};

void
e1000_init(void)
{
    printk("e1000: LevOS 7.0 driver\n");
}

void
e1000_write_cmd(struct e1000_device *edev, uint16_t addr, uint32_t val)
{
    if (edev->bar_type == 0) {
        *(uint32_t *)(edev->mbase + addr) = val;
    } else {
        panic("e1000: io ports are not supported\n");
        /*Ports::outportl(io_base, p_address);
        Ports::outportl(io_base + 4, p_value);*/
    }
}
uint32_t
e1000_read_cmd(struct e1000_device *edev, uint16_t addr)
{
    if (edev->bar_type == 0) {
        return *(uint32_t *)(edev->mbase + addr);
    } else {
        /*Ports::outportl(io_base, p_address);
        return Ports::inportl(io_base + 4);*/
        panic("e1000: io ports are not supported\n");
        return 0;
    }
}

uint32_t
e1000_read_eeprom(struct e1000_device *edev, uint8_t addr)
{
    uint16_t data = 0;
    uint32_t tmp = 0;
    e1000_write_cmd(edev, REG_EEPROM, (1) | ((uint32_t)(addr) << 8));

    while(!((tmp = e1000_read_cmd(edev, REG_EEPROM)) & (1 << 4)))
        ;

    data = (uint16_t)((tmp >> 16) & 0xFFFF);
	return data;
}

int
e1000_read_mac(struct e1000_device *edev, uint8_t *mac)
{
    uint32_t temp;

    temp = e1000_read_eeprom(edev, 0);
    mac[0] = temp &  0xff;
    mac[1] = temp >> 8;
    temp = e1000_read_eeprom(edev, 1);
    mac[2] = temp &  0xff;
    mac[3] = temp >> 8;
    temp = e1000_read_eeprom(edev, 2);
    mac[4] = temp &  0xff;
    mac[5] = temp >> 8;
    
    return 0;
}

int
e1000_probe(struct pci_device *pdev)
{
    printk("e1000: probing\n");
    return 0;
}


void e1000_rxinit(struct e1000_device *edev)
{
    uint8_t *ptr, *v_ptr;
    struct e1000_rx_desc *descs;
 
    // Allocate buffer for receive descriptors. For simplicity, in my case khmalloc returns a virtual address that is identical to it physical mapped address.
    // In your case you should handle virtual and physical addresses as the addresses passed to the NIC should be physical ones
 
    //ptr = (uint8_t *)(kmalloc_ptr->khmalloc(sizeof(struct e1000_rx_desc)*E1000_NUM_RX_DESC + 16));
    ptr = (void *) palloc_get_page();
    v_ptr = kmap_map_page((uint32_t) ptr);
 
    descs = (struct e1000_rx_desc *) v_ptr;
    for(int i = 0; i < E1000_NUM_RX_DESC; i++) {
        edev->rx_descs[i] = (struct e1000_rx_desc *) ((uint8_t *)descs + i*16);
        //rx_descs[i]->addr = (uint64_t)(uint8_t *) (kmalloc_ptr->khmalloc(8192 + 16));
        edev->rx_descs[i]->addr = (uint64_t)(uint32_t)(uint8_t *)(kv2p(malloc(8192 + 16)));
        edev->rx_descs[i]->status = 0;
    }
 
    e1000_write_cmd(edev, REG_TXDESCLO, (uint32_t)(((uint64_t)(uint32_t)ptr) >> 32) );
    e1000_write_cmd(edev, REG_TXDESCHI, (uint32_t)(((uint64_t)(uint32_t)ptr) & 0xFFFFFFFF));
 
    e1000_write_cmd(edev, REG_RXDESCLO, (uint64_t)(uint32_t)ptr);
    e1000_write_cmd(edev, REG_RXDESCHI, 0);
 
    e1000_write_cmd(edev, REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);
 
    e1000_write_cmd(edev, REG_RXDESCHEAD, 0);
    e1000_write_cmd(edev, REG_RXDESCTAIL, E1000_NUM_RX_DESC-1);
    edev->rx_cur = 0;
    e1000_write_cmd(edev, REG_RCTRL,
            RCTL_EN| RCTL_SBP| RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE |
            RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC  | RCTL_BSIZE_2048);
}

void
e1000_txinit(struct e1000_device *edev)
{    
    uint8_t *ptr, *v_ptr;
    struct e1000_tx_desc *descs;
    // Allocate buffer for receive descriptors. For simplicity, in my case khmalloc returns a virtual address that is identical to it physical mapped address.
    // In your case you should handle virtual and physical addresses as the addresses passed to the NIC should be physical ones
    //ptr = (uint8_t *)(kmalloc_ptr->khmalloc(sizeof(struct e1000_tx_desc)*E1000_NUM_TX_DESC + 16));
    ptr = (void *) palloc_get_page();
    v_ptr = kmap_map_page((uint32_t) ptr);
 
    descs = (struct e1000_tx_desc *)v_ptr;
    for(int i = 0; i < E1000_NUM_TX_DESC; i++)
    {
        edev->tx_descs[i] = (struct e1000_tx_desc *)((uint8_t*)descs + i*16);
        edev->tx_descs[i]->addr = 0;
        edev->tx_descs[i]->cmd = 0;
        edev->tx_descs[i]->status = TSTA_DD;
    }
 
    e1000_write_cmd(edev, REG_TXDESCHI, (uint32_t)(((uint64_t)(uint32_t)ptr) >> 32) );
    e1000_write_cmd(edev, REG_TXDESCLO, (uint32_t)(((uint64_t)(uint32_t)ptr) & 0xFFFFFFFF));
 
    //now setup total length of descriptors
    e1000_write_cmd(edev, REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);
 
 
    //setup numbers
    e1000_write_cmd(edev, REG_TXDESCHEAD, 0);
    e1000_write_cmd(edev, REG_TXDESCTAIL, 0);
    edev->tx_cur = 0;
    e1000_write_cmd(edev, REG_TCTRL,  TCTL_EN
        | TCTL_PSP
        | (15 << TCTL_CT_SHIFT)
        | (64 << TCTL_COLD_SHIFT)
        | TCTL_RTLC);
 
    // This line of code overrides the one before it but I left both to highlight that the previous one works with e1000 cards, but for the e1000e cards 
    // you should set the TCTRL register as follows. For detailed description of each bit, please refer to the Intel Manual.
    // In the case of I217 and 82577LM packets will not be sent if the TCTRL is not configured using the following bits.
    //writeCommand(REG_TCTRL,  0b0110000000000111111000011111010);
    //writeCommand(REG_TIPG,  0x0060200A);
 }

void
e1000_enable_irq(struct e1000_device *edev)
{
    e1000_write_cmd(edev, REG_IMASK, 0x1F6DC);
    e1000_write_cmd(edev, REG_IMASK, 0xff & ~4);
    e1000_read_cmd(edev, 0xc0);
}

void
e1000_start_link(struct e1000_device *edev)
{
    uint32_t val;
	val = e1000_read_cmd(edev, REG_CTRL);

	e1000_write_cmd(edev, REG_CTRL, val | ECTRL_SLU);
}

void
e1000_handle_receive(struct e1000_device *edev)
{
    uint16_t old_cur;
    bool got_packet = false;
 
    while((edev->rx_descs[edev->rx_cur]->status & 0x1)) {
        got_packet = true;
        /* FIXME: ideally check if addr fits in the 32bit address space */
        uint8_t *buf = (uint8_t *)(uint32_t) edev->rx_descs[edev->rx_cur]->addr;
        uint16_t len = edev->rx_descs[edev->rx_cur]->length;

        /* copy the packet to a kernel buffer */
        void *kbuf = malloc(len);
        if (!kbuf) {
            /* drop the packet */
            goto drop;
        }

        if (len > 0x1000) {
            printk("CRITICAL: large packet received, dropped\n");
            free(kbuf);
            goto drop;
        }

        uintptr_t p_page = PG_RND_DOWN((uint32_t)buf);

        if ((uint32_t) buf + len > p_page + 0x1000) {
            printk("CRITICAL: overlapping page :(\n");
            goto drop;
        }

        void *tmpmap = kmap_map_page(p_page);
        memcpy(kbuf, (void *)((uintptr_t) tmpmap + ((uintptr_t) buf - p_page)), len);

        /* FIXME: free tmpmap */
        kmap_free_page(tmpmap);

        packet_push_queue(&edev->ndev.ndev_ni, kbuf, len);

        /* ack the packet */
drop:
        edev->rx_descs[edev->rx_cur]->status = 0;
        old_cur = edev->rx_cur;
        edev->rx_cur = (edev->rx_cur + 1) % E1000_NUM_RX_DESC;
        e1000_write_cmd(edev, REG_RXDESCTAIL, old_cur);
    }    
}

void
e1000_irq_handler(struct pt_regs *regs)
{
    //printk("e1000: IRQ\n");
    struct e1000_device *edev = intr_get_priv(regs->vec_no);

    uint32_t status = e1000_read_cmd(edev, 0xc0);
    if(status & 0x04)
    {
        printk("start link\n");
    }
    else if(status & 0x10)
    {
        printk("good threshold\n");
    }
    else if(status & 0x80)
    {
        e1000_handle_receive(edev);
    }
}

void
__e1000_send_packet(struct e1000_device *edev, const void *p_data, uint16_t p_len)
{    
    edev->tx_descs[edev->tx_cur]->addr = (uint64_t)kv2p((void *) p_data);
    edev->tx_descs[edev->tx_cur]->length = p_len;
    edev->tx_descs[edev->tx_cur]->cmd = CMD_EOP | CMD_IFCS | CMD_RS | CMD_RPS;
    edev->tx_descs[edev->tx_cur]->status = 0;
    uint8_t old_cur = edev->tx_cur;
    edev->tx_cur = (edev->tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write_cmd(edev,REG_TXDESCTAIL, edev->tx_cur);   
    while(!(edev->tx_descs[old_cur]->status & 0xff));    
}

void
e1000_send_packet(struct e1000_device *edev, packet_t *pkt)
{
    __e1000_send_packet(edev, pkt->p_buf, pkt->p_len);
}

int
e1000_net_send_packet(struct net_device *ndev, packet_t *pkt)
{
    struct e1000_device *edev = container_of(ndev, struct e1000_device, ndev);
    
    e1000_send_packet(edev, pkt);

    return 0;
}

int
e1000_net_up(struct net_device *ndev)
{
    struct e1000_device *edev = container_of(ndev, struct e1000_device, ndev);
    struct net_info *ni = &ndev->ndev_ni;

    //e1000_enable_irq(edev);

    if (ndev->ndev_flags & NDEV_FLAG_DHCP) {
        net_printk("DOING DHCP\n");
        do_dhcp(ndev);
    } else if (ndev->ndev_flags & NDEV_FLAG_STATIC) {
        /* TODO: set static IP */
    } else {
        net_printk("Forced link local address!\n");
        ni->ni_src_ip = IP(169, 254, 13, 37);
        ni->ni_dhcp_state = NI_DHCP_STATE_VALID;
        ni->ni_arp_kick = 1;
    }

    //test_tcp(ni);

    return 0;
}

void
e1000_init_ndev(struct net_device *ndev)
{
    memset(ndev, 0, sizeof(*ndev));
    ndev->ndev_flags = 0;
    ndev->send_packet = e1000_net_send_packet;
    ndev->up = e1000_net_up;
    ndev->down = NULL;
}

int
e1000_attach(struct pci_device *pdev)
{
    uint32_t bar0 = pci_device_get_bar0(pdev);
    uint8_t mac[6];

    printk("e1000: bar0 at 0x%x\n", bar0);
    struct e1000_device *edev = malloc(sizeof(*edev));
    if (!edev)
        return -ENOMEM;

    memset(edev, 0, sizeof(*edev));

    edev->bar_type = bar0 & 1;
    edev->mbase = bar0;
    edev->pdev = pdev;
    pdev->priv = edev;

    pci_enable_busmaster(pdev);

    /* FIXME: this is ugly, but map the bar into the kernel */
    map_page_kernel(edev->mbase, edev->mbase, 1);
    map_page_kernel(edev->mbase + 0x1000, edev->mbase + 0x1000, 1);
    map_page_kernel(edev->mbase + 0x2000, edev->mbase + 0x2000, 1);
    map_page_kernel(edev->mbase + 0x3000, edev->mbase + 0x3000, 1);
    map_page_kernel(edev->mbase + 0x4000, edev->mbase + 0x4000, 1);
    map_page_kernel(edev->mbase + 0x5000, edev->mbase + 0x5000, 1);

    /* read the MAC address into the device */
    e1000_read_mac(edev, edev->mac);

    /*printk("e1000: MAC address: %x:%x:%x:%x:%x:%x\n", edev->mac[0],
                edev->mac[1], edev->mac[2], edev->mac[3], edev->mac[4],
                edev->mac[5]);*/
    printk("e1000: MAC address: %pE\n", edev->mac);

    printk("e1000: initializing "__stringify(E1000_NUM_RX_DESC)" RX buffers\n");
    e1000_rxinit(edev);

    printk("e1000: initializing "__stringify(E1000_NUM_TX_DESC)" TX buffers\n");
    e1000_txinit(edev);

    e1000_start_link(edev);

    for(int i = 0; i < 0x80; i++)
        e1000_write_cmd(edev, 0x5200 + i*4, 0);

    uint8_t irqline = (uint8_t) pci_device_get_irqline(pdev);
    uint8_t finalirq = (uint8_t)0x20 + irqline;

    printk("e1000: using IRQ %d mapped to INT %d\n", irqline, finalirq);

    intr_set_priv(finalirq, edev);
    intr_register_hw(finalirq, e1000_irq_handler);

    packet_t *packet;

    e1000_init_ndev(&edev->ndev);

    struct net_info *ni = &edev->ndev.ndev_ni;
    net_info_init(ni);

    memcpy(ni->ni_hw_mac, edev->mac, 6);
    ni->ni_src_ip = IP(0, 0, 0, 0);

    //packet = arp_construct_request_eth_ip(edev->mac, IP(169, 254, 13, 37), IP(192,168,0,137));
    //e1000_send_packet(edev, packet);

    //packet = ip_construct_packet_eth(edev->mac, IP(169, 254, 13, 37), IP(255,255,255,255));
    //packet = udp_new_packet(ni, 1337, IP(255, 255, 255, 255), 7548);

    //char *testpayload = "Hello Internet from LevOS 7.0!";
    //udp_set_payload(packet, testpayload, strlen(testpayload));

    //e1000_send_packet(edev, packet);
    e1000_net_up(&edev->ndev);
    net_register_device(&edev->ndev);
    return 1;
}

static const struct pci_ident e1000_pci_idents[] = {
    { .pci_vendor = 0x8086, .pci_device = 0x100E},
    { .pci_vendor = 0x8086, .pci_device = 0x153A},
    { .pci_vendor = 0x8086, .pci_device = 0x10EA},
    PCI_END_IDENT,
};

struct pci_driver e1000_driver = {
    .name = "e1000",
    .init = e1000_init,
    .probe = e1000_probe,
    .attach = e1000_attach,
    .idents = (void *) &e1000_pci_idents,
};
