#ifndef __LEVOS_WORK_H
#define __LEVOS_WORK_H

#include <levos/types.h>
#include <levos/list.h>

#define WORK_FLAG_CANCELLED (1 << 0)
#define WORK_FLAG_QUEUED    (1 << 1)
#define WORK_FLAG_KILLED    (1 << 2)

struct work {
    void (*work_func)(void *);
    void *work_aux;

    uint32_t work_at;

    int work_flags;

    struct list_elem elem;
};

int work_init(void);

/* scheduling work */
int schedule_work(struct work *);
int schedule_work_at(struct work *, uint32_t);
int schedule_work_delay(struct work *, uint32_t);

/* rescheduling work */
int work_reschedule(uint32_t);

/* cancelling work */
int work_cancel(struct work *);
int work_cancel_current(void);


/* creating work */
struct work *work_create(void (*)(void *), void *);

#endif /* __LEVOS_WORK_H */
