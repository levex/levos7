#ifndef __LEVOS_SOCKET_H
#define __LEVOS_SOCKET_H

#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/ip.h>
#include <levos/packet.h>
#include <levos/fs.h>

struct socket;

typedef size_t socklen_t;

struct sockaddr_in {
    int sin_family;
    be_port_t sin_port;
    ip_addr_t sin_addr;
};

struct sockaddr {
    uint32_t SOCKADDR_DONT_USE_ME;
};

struct socket_ops {
    int (*connect)(struct socket *, struct sockaddr *, socklen_t);
    int (*write)(struct socket *, void *buf, size_t len);
    int (*destroy)(struct socket *);
};

#define AF_UNIX 0
#define AF_LOCAL AF_UNIX
#define AF_INET 1

#define SOCK_STREAM 0
#define SOCK_DGRAM  1

struct socket {
    //struct file sock_file;
    int sock_domain;
    int sock_type;
    int sock_proto;

    struct net_info *sock_ni;

    void *sock_priv;
    struct socket_ops *sock_ops;
};

void net_init(void);
int net_register_device(struct net_device *);
struct net_device *net_get_default();
port_t net_allocate_port(int);
void net_free_port(int, port_t);

struct socket *socket_new(int, int, int);
void socket_destroy(struct socket *);

struct file *file_from_socket(struct socket *);
#endif
