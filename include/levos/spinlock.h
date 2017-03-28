#ifndef __LEVOS_SPINLOCK_H
#define __LEVOS_SPINLOCK_H

#include <levos/compiler.h>
#include <levos/task.h>

struct __spinlock_t {
    volatile int value;
    struct task *holder;
} __packed;

typedef struct __spinlock_t spinlock_t;

void spin_lock_init(spinlock_t *);
void spin_lock(spinlock_t *);
void spin_unlock(spinlock_t *);

#endif /* __LEVOS_SPINLOCK_H */
