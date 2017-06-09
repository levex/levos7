#ifndef __LEVOS_PIPE_H
#define __LEVOS_PIPE_H

#include <levos/types.h>
#include <levos/fs.h>
#include <levos/ring.h>
#include <levos/spinlock.h>

/* XXX: this is 65536 on Linux */
#define PIPE_BUF 4096

#define PIPFLAG_WRITE_CLOSED (1 << 0)
#define PIPFLAG_READ_CLOSED  (1 << 1)

struct pipe {
    struct file *pipe_read;
    struct file *pipe_write;
    struct ring_buffer pipe_buffer;
    spinlock_t pipe_lock;
    volatile int pipe_flags;
};

#endif /* __LEVOS_PIPE_H */
