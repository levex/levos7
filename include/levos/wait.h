#ifndef __LEVOS_WAIT_H
#define __LEVOS_WAIT_H

#include <levos/types.h>
#include <levos/list.h>

struct wait_queue_struct {
    struct list wq_waiters;
    int         wq_num;
};

typedef struct wait_queue_struct wait_queue_t;

#endif /* __LEVOS_WAIT_H */
