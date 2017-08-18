#include <levos/kernel.h>
#include <levos/arp.h>
#include <levos/eth.h>
#include <levos/packet.h>
#include <levos/spinlock.h>
#include <levos/list.h>
#include <levos/tcp.h>
#include <levos/work.h>
#include <levos/e1000.h> /* FIXME: make it net_device eventually */

#ifdef CONFIG_ETH_DEBUG
#define net_printk printk
#else
#define net_printk(...) ;
#endif

static struct list packet_list;
static spinlock_t packet_list_lock;

struct packet_desc {
    void *pdata;
    size_t plen;
    struct net_info *ni;
    struct list_elem elem;
};

packet_t *packet_allocate()
{
    packet_t *pkt = malloc(sizeof(*pkt));
    if (!pkt)
        return NULL;

    pkt->p_len = 0;
    pkt->p_buf = NULL;
    pkt->p_ptr = 0;

    return pkt;
}

int
packet_grow(packet_t *pkt, int len)
{
    pkt->p_buf = realloc(pkt->p_buf, pkt->p_len + len);
    if (!pkt->p_buf)
        return -ENOMEM;
    pkt->p_ptr = pkt->p_buf + pkt->p_len;

    if (pkt->p_ptr == NULL)
        pkt->p_ptr = pkt->p_buf;

    pkt->p_len += len;

    return 0;
}

void
packet_destroy(packet_t *pkt)
{
    if (!pkt)
            return;

    free(pkt->p_buf);
    free(pkt);
}

struct packet_retransmission_descriptor {
    packet_t *pkt;
    int tries_left;
    int delay;
    void (*notify_retransmit_failed)(struct net_info *, packet_t *);
    struct net_info *ni;
};

void
handle_failed_transmission(struct net_info *ni, struct packet_retransmission_descriptor *desc)
{
    net_printk("%s: failed to retransmit a packet, no tries left\n", __func__);
    desc->notify_retransmit_failed(ni, desc->pkt);
    free(desc);
}

void
do_packet_retransmit(void *opaque)
{
    struct work *work;
    struct packet_retransmission_descriptor *desc = opaque;
    struct net_device *ndev = container_of(desc->ni, struct net_device, ndev_ni);

    ndev->send_packet(ndev, desc->pkt);
    net_printk("%s: retransmitted a packet\n", __func__);

    desc->tries_left --;

    if (desc->tries_left == 0) {
        handle_failed_transmission(desc->ni, desc);
        return;
    }

    work_reschedule(desc->delay);
}

struct work *
packet_schedule_retransmission(struct net_info *ni,
                                packet_t *pkt,
                                int maxtries, int delay,
                                void (*notify_retransmit_failed)(struct net_info *, packet_t *))
{
    struct work *work;

    struct packet_retransmission_descriptor *desc = malloc(sizeof(*desc));
    if (!desc)
        return NULL;

    desc->pkt = pkt;
    desc->ni = ni;
    desc->delay = delay;
    desc->tries_left = maxtries;
    desc->notify_retransmit_failed = notify_retransmit_failed;

    work = work_create(do_packet_retransmit, desc);
    if (!work)
        return NULL;

    schedule_work_delay(work, delay);

    return work;
}

extern unsigned udp_hash_usp(const struct hash_elem *e, void *aux);
extern bool udp_less_usp(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void
net_info_init(struct net_info *ni)
{
    memset(ni, 0, sizeof(*ni));

    hash_init(&ni->ni_tcp_infos, tcp_hash_tcp_info, tcp_less_tcp_info, NULL);
    hash_init(&ni->ni_udp_sockets, udp_hash_usp, udp_less_usp, NULL);
    spin_lock_init(&ni->ni_tcp_infos_lock);
}

/* THIS IS CALLED IN IRQ CONTEXT */
void
packet_push_queue(struct net_info *ni, void *packet, size_t len)
{
    struct packet_desc *desc = malloc(sizeof(*desc));
    if (!desc)
        return;

    desc->pdata = packet;
    desc->plen = len;
    desc->ni = ni;

    spin_lock(&packet_list_lock);

    list_push_back(&packet_list, &desc->elem);

    spin_unlock(&packet_list_lock);
}

void
do_handle_packet(struct net_info *ni, packet_t *pkt)
{
    int rc;
    uint16_t eth_proto;

    eth_dump_packet(pkt);

    if (eth_should_drop(ni, pkt->p_ptr))
        goto drop;

    eth_proto = eth_get_proto(pkt->p_ptr);

    pkt->p_ptr += sizeof(struct ethernet_header);
    pkt->pkt_ip_offset = pkt->p_ptr - pkt->p_buf;

    /* TODO: handle the return codes here */
    if (eth_proto == ETH_TYPE_IP4) {
        net_printk("^ ip4\n");
        ip_handle_packet(ni, pkt, pkt->p_ptr);
        goto handled;
    } else if (eth_proto == ETH_TYPE_ARP) {
        net_printk("^ arp\n");
        arp_handle_packet(ni, pkt, pkt->p_ptr);
        goto handled;
    }

    net_printk("^ unknown\n");
    goto drop;

    /* tralalalal */

handled:
    goto free;
drop:
    //printk("dropping a packet\n");
    /* TODO drop packet */
    net_printk("^ dropped\n");
    goto free;
free:
    //printk("BEFORE:\n");
    //heap_proc_heapstats(0, 0, NULL, 0);
    packet_destroy(pkt);
    //printk("AFTER:\n");
    //heap_proc_heapstats(0, 0, NULL, 0);
}

void
handle_packet(struct packet_desc *packet)
{
    struct net_info *ni;
    packet_t *pkt = malloc(sizeof(*pkt));
    if (!pkt) {
        printk("CRITICAL: dropped a packet due to OOM\n");
        return;
    }

    /* setup the packet, reusing the buffer */
    pkt->p_buf = packet->pdata;
    pkt->p_ptr = packet->pdata;
    pkt->p_len = packet->plen;
    pkt->pkt_payload_offset = 0;
    pkt->pkt_ip_offset = 0;
    pkt->pkt_proto_offset = 0;
    
    ni = packet->ni;

    /* free the descriptor */
    free(packet);

    /* handle the packet now */
    do_handle_packet(ni, pkt);
}

void
packet_processor_thread()
{
    printk("packethandler: process spawned\n");
    list_init(&packet_list);
    spin_lock_init(&packet_list_lock);

    while (1) {
        if (list_empty(&packet_list)) {
            sched_yield();
            continue;
        }

        spin_lock(&packet_list_lock);
        struct packet_desc *desc
            = list_entry(list_pop_front(&packet_list),
                         struct packet_desc,
                         elem);
        spin_unlock(&packet_list_lock);

        handle_packet(desc);
    }
}
