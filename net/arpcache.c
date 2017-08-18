#include <levos/kernel.h>
#include <levos/arp.h>
#include <levos/hash.h>
#include <levos/list.h>
#include <levos/work.h>
#include <levos/task.h>

static struct hash arpcache;

struct arp_cache_entry {
    eth_addr_t ace_eth;
    ip_addr_t ace_ip;
    uint32_t ace_expiry;
    struct hash_elem helem;
};

static bool
arp_hash_ip_less(const struct hash_elem *ha,
                 const struct hash_elem *hb,
                 void *aux)
{
    struct arp_cache_entry *a = hash_entry(ha, struct arp_cache_entry, helem);
    struct arp_cache_entry *b = hash_entry(hb, struct arp_cache_entry, helem);

    return a->ace_ip < b->ace_ip;
}

static unsigned
arp_hash_ip(const struct hash_elem *ha, void *aux)
{
    struct arp_cache_entry *a = hash_entry(ha, struct arp_cache_entry, helem);

    return hash_int(a->ace_ip);
}

static void
arp_remove_entry_work(void *v)
{
    struct arp_cache_entry *entry = v;

    hash_delete(&arpcache, &entry->helem);

    free(entry);
}

int
arp_cache_insert(ip_addr_t ip, eth_addr_t eth)
{
    struct work *work;
    struct arp_cache_entry *entry = malloc(sizeof(*entry));
    if (!entry)
        return -ENOMEM;

    memcpy(entry->ace_eth, eth, 6);
    entry->ace_ip = ip;
    entry->ace_expiry = 1000000;
    hash_insert(&arpcache, &entry->helem);

    /* schedule work to remove it */
    work = work_create(arp_remove_entry_work, entry);
    schedule_work_delay(work, entry->ace_expiry);

    net_printk("%s: %pI is at %pE\n", __func__, ip, eth);

    return 0;
}

uint8_t *
arp_do_request_eth(struct net_info *ni, ip_addr_t ip)
{
    packet_t *pkt;
    struct net_device *ndev = NDEV_FROM_NI(ni);
    int delay = 10000;
    struct hash_elem *entry;
    struct arp_cache_entry ace;

    ace.ace_ip = to_be_32(ip);

    pkt = arp_construct_request_eth_ip(ni->ni_hw_mac, ni->ni_src_ip, ip);
    if (!pkt)
        return NULL;

    /* send the packet */
    ndev->send_packet(ndev, pkt);

    entry = hash_find(&arpcache, &ace.helem);
    while (entry == NULL) {

        memset(&ace, 0, sizeof(ace));
        ace.ace_ip = to_be_32(ip);

        delay  --;
        if (delay <= 0)
            return NULL;

        sched_yield();

        entry = hash_find(&arpcache, &ace.helem);
    }
    return hash_entry(entry, struct arp_cache_entry, helem)->ace_eth;
}

uint8_t *
arp_get_eth_addr(struct net_info *ni, ip_addr_t ip)
{
    struct arp_cache_entry entry;
    entry.ace_ip = to_be_32(ip);

    net_printk("%s: %pI\n", __func__, entry.ace_ip);

    struct hash_elem *helem = hash_find(&arpcache, &entry.helem);
    if (helem == NULL) {
        /* not in the cache, send request, wait then add to cache */
        net_printk("%s: cache miss\n", __func__);
        return arp_do_request_eth(ni, ip);
    } else {
        /* element was in the cache, return the ethernet address */
        struct arp_cache_entry *entry = hash_entry(helem, struct arp_cache_entry, helem);
        net_printk("%s: returned %pE\n", __func__, entry->ace_eth);
        return entry->ace_eth;
    }
}

int
arp_cache_init(void)
{
    hash_init(&arpcache, arp_hash_ip, arp_hash_ip_less, NULL);
    printk("arpcache: initialized ARP cache\n");
}
