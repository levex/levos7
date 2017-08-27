#include <levos/packet.h>
#include <levos/device.h>
#include <levos/e1000.h>
#include <levos/socket.h>
#include <levos/ip.h>
#include <levos/udp.h>
#include <levos/arp.h>
#include <levos/bitmap.h>

struct list net_devices_list;
spinlock_t net_devices_lock;

struct list route_table;
spinlock_t route_table_lock;

spinlock_t port_lock;
struct bitmap *dgram_port_bitmap;
struct bitmap *stream_port_bitmap;

static struct net_device *__default_ndev;

void
net_init()
{
    /* initalize ARP cache */
    arp_cache_init();

    /* initialize devices */
    list_init(&net_devices_list);
    spin_lock_init(&net_devices_lock);

    /* initialize routing table */
    list_init(&route_table);
    spin_lock_init(&route_table_lock);

    /* initialize srcport allocation */
    dgram_port_bitmap = bitmap_create(65535);
    stream_port_bitmap = bitmap_create(65535);

    panic_ifnot(dgram_port_bitmap != NULL && stream_port_bitmap != NULL);

    /* reserve privileged ports */
    bitmap_set_multiple(dgram_port_bitmap, 0, 1000, true);
    bitmap_set_multiple(stream_port_bitmap, 0, 1000, true);

    printk("net: initialized infrastructure\n");
}

/* TODO: implement routing table */

int
net_add_route(struct net_device *iface, ip_addr_t base, ip_addr_t netmask,
        ip_addr_t gateway)
{
    return -ENOSYS;
}

struct net_device *
net_find_route(ip_addr_t target)
{
    return net_get_default();
}

struct net_info *
__route_find_ni_for_dstip(ip_addr_t dstip)
{
    return &net_find_route(dstip)->ndev_ni;
}

struct net_info *
route_find_ni_for_dst(uint32_t dstip)
{
    return __route_find_ni_for_dstip(dstip);
}

void
net_set_default(struct net_device *ndev)
{
    __default_ndev = ndev;
    __default_ndev->ndev_flags |= NDEV_FLAG_DEFAULT;
}

struct net_device *
net_get_default()
{
    return __default_ndev;
}

int
net_register_device(struct net_device *ndev)
{
    /* by default, we want to set DHCP and not static */
    ndev->ndev_flags  =  NDEV_FLAG_DHCP;
    //ndev->ndev_flags &= ~NDEV_FLAG_STATIC;

    int no = 0;

    spin_lock(&net_devices_lock);
    list_push_back(&net_devices_list, &ndev->elem);
    no = list_size(&net_devices_list) - 1;
    spin_unlock(&net_devices_lock);

    if (no == 0)
        net_set_default(ndev);

    printk("net: registered network device as en%d\n", no);
}

int
socket_udp_create(struct socket *sock, int proto)
{
    if (proto != 0)
        return -EINVAL;

    sock->sock_proto = 0;
    sock->sock_type = SOCK_DGRAM;
    sock->sock_ops = &udp_sock_ops;
    sock->sock_priv = NULL;

    return 0;
}

extern struct socket_ops tcp_sock_ops;
int
socket_tcp_create(struct socket *sock, int proto)
{
    if (proto != 0)
        return -EINVAL;

    sock->sock_proto = 0;
    sock->sock_type = SOCK_STREAM;
    sock->sock_ops = &tcp_sock_ops;
    sock->sock_priv = NULL;

    return 0;
}

int
socket_inet_create(struct socket *sock, int type, int proto)
{
    sock->sock_domain = AF_INET;

    switch(type) {
        case SOCK_DGRAM:
            return socket_udp_create(sock, proto);
        case SOCK_STREAM:
            return socket_tcp_create(sock, proto);
        default:
            return -EINVAL;
    }
}

struct socket *
socket_new(int dom, int type, int proto)
{
    struct socket *sock;
    int rc;

    sock = malloc(sizeof(*sock));
    if (!sock)
        return NULL;

    switch(dom) {
        case AF_INET:
            rc = socket_inet_create(sock, type, proto);
            if (rc) {
                free(sock);
                return (void *) rc;
            }
            return sock;
        default:
            free(sock);
            return (void *) -EAFNOSUPPORT;
    }
}

void
socket_destroy(struct socket *sock)
{
    sock->sock_ops->destroy(sock);
    free(sock);
}

size_t
socket_fs_read(struct file *filp, void *buf, size_t len)
{
    struct socket *sock = filp->priv;

    return sock->sock_ops->read(sock, buf, len);
}

size_t
socket_fs_write(struct file *filp, void *buf, size_t len)
{
    struct socket *sock = filp->priv;

    return sock->sock_ops->write(sock, buf, len);
}

int
socket_fs_fstat(struct file *filp, struct stat *buf)
{
    /* TODO */

    buf->st_dev = 0;
    buf->st_ino = 0;
    buf->st_mode = 0;
    buf->st_nlink = 1;
    buf->st_uid = buf->st_gid = 0;
    buf->st_rdev = 0;
    buf->st_size = 0;
}

int
socket_fs_close(struct file *filp)
{
    struct socket *sock = filp->priv;

    socket_destroy(sock);
    free(filp);
}

struct file_operations socket_fops = {
    .read = socket_fs_read,
    .write = socket_fs_write,
    .close = socket_fs_close,
};

/* wraps a socket in a struct file for inclusion in the filetable */
struct file *
file_from_socket(struct socket *sock)
{
    struct file *filp = malloc(sizeof(*filp));
    if (!filp)
        return NULL;

    filp->type = FILE_TYPE_SOCKET;
    filp->isdir = 0;
    filp->fpos = 0;
    filp->respath = NULL;
    filp->priv = sock;
    filp->fops = &socket_fops;
    filp->refc = 1;

    return filp;
}

port_t
net_allocate_port(int family)
{
    int loc;

    if (family == SOCK_DGRAM)
        loc = bitmap_scan_and_flip(dgram_port_bitmap, 0, 1, false);
    else if (family == SOCK_STREAM)
        loc = bitmap_scan_and_flip(stream_port_bitmap, 0, 1, false);
    else
        return -1;

    if (loc == BITMAP_ERROR)
        return -1;

    return (port_t) loc;
}

void
net_free_port(int family, port_t port)
{
    if (family == SOCK_DGRAM)
        bitmap_set(dgram_port_bitmap, port, false);
    else if (family == SOCK_STREAM)
        bitmap_set(stream_port_bitmap, port, false);
    else
        panic("Invalid family submitted to %s", __func__);

}
