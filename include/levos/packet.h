#ifndef __LEVOS_PACKET_H
#define __LEVOS_PACKET_H

#include <levos/types.h>
#include <levos/hash.h>
#include <levos/spinlock.h>

#define PACKET_DROP 0
#define PACKET_HANDLED 1
#define PACKET_COULDNTHANDLE 2

#ifdef CONFIG_NET_DEBUG
# define net_printk printk
#else
# define net_printk(...) ;
#endif

typedef uint16_t port_t;
typedef be_uint16_t be_port_t;

typedef struct {
    void *p_buf; // base pointer
    void *p_ptr; // Current header ptr
    uint32_t p_len; // length of the packet
    uintptr_t pkt_ip_offset;
    uintptr_t pkt_proto_offset;
    uintptr_t pkt_payload_offset;
} packet_t;

#define NI_DHCP_STATE_NULL     0 /* unknown, or no dhcp */
#define NI_DHCP_STATE_DISCOVER 1 /* we've sent the discovery */
#define NI_DHCP_STATE_OFFER    2 /* we received an offer */
#define NI_DHCP_STATE_VALID    3 /* ni_src_ip is valid and based on DHCP */

struct net_info {
    uint8_t ni_hw_mac[6]; /* our HW/MAC address */
    uint32_t ni_src_ip; /* our IPv4 address */
    int ni_dhcp_state; /* where are we in the DHCP state machine */
    int ni_dhcp_tries; /* how many tries we have left to successfully do DHCP */
    int ni_arp_kick;  /* whether ARP requests should be replied */
    struct hash ni_tcp_infos; /* hashtable (key: ti_src_port) containing the
                                 current state of TCP connections
                               */
    spinlock_t ni_tcp_infos_lock; /* lock for the table */
};
void net_info_init(struct net_info *);

#define NDEV_FLAG_ROUTED (1 << 0)  /* entries added to the routing table? */
#define NDEV_FLAG_ACTIVE (1 << 1)  /* packet processing activated? */
#define NDEV_FLAG_HASIP  (1 << 2)  /* has an IP address? */
#define NDEV_FLAG_DHCP   (1 << 3)  /* IP via DHCP */
#define NDEV_FLAG_STATIC (1 << 4)  /* static IP */
#define NDEV_FLAG_DEFAULT (1 << 5) /* this is the default IF */

struct net_device {
    struct net_info ndev_ni;

    int ndev_flags;

    int (*send_packet)(struct net_device *, packet_t *);
    int (*up)(struct net_device *);
    int (*down)(struct net_device *);

    struct list_elem elem;
};

#define NDEV_FROM_NI(ni) container_of(ni, struct net_device, ndev_ni)

/* FIXME: these shoudl be ip_addr_t's */
struct route_entry {
    uint32_t re_base;
    uint32_t re_netmask;
    uint32_t re_gateway;

    struct net_device *re_ndev;
};

struct net_info *
route_find_ni_for_dst(uint32_t);

inline be_uint16_t
to_be_16(uint16_t nb)
{
    return (nb>>8) | (nb<<8);
}

inline uint16_t
to_le_16(be_uint16_t nb)
{
    return (uint16_t) to_be_16((uint16_t) nb);
}
   
inline be_uint32_t
to_be_32(uint32_t nb) {
    return ((nb>>24)&0xff)      |
            ((nb<<8)&0xff0000)   |
            ((nb>>8)&0xff00)     |
            ((nb<<24)&0xff000000);
}

inline uint32_t
to_le_32(be_uint32_t nb)
{
    return (uint32_t) to_be_32((uint32_t) nb);
}

void packet_processor_thread();

packet_t *packet_allocate(void);
int packet_grow(packet_t *, int);
void packet_destroy(packet_t *);
void packet_push_queue(struct net_info *, void *, size_t);

struct work *
packet_schedule_retransmission(struct net_info *ni,
                                packet_t *pkt,
                                int maxtries, int delay,
                                void (*notify_retransmit_failed)(struct net_info *, packet_t *));

#endif /* __LEVOS_PACKET_H */
