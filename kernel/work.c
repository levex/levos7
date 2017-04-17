#include <levos/kernel.h>
#include <levos/work.h>
#include <levos/list.h>
#include <levos/spinlock.h>
#include <levos/task.h>

struct list work_list;
spinlock_t work_lock;
struct work *current_work;

extern uint32_t __pit_ticks;

struct task *worker_task;

uint32_t
work_get_ticks()
{
    /* XXX: x86 specific */
    return __pit_ticks;
}

bool
work_less(const struct list_elem *ea, const struct list_elem *eb, void *aux)
{
    struct work *a = list_entry(ea, struct work, elem);
    struct work *b = list_entry(eb, struct work, elem);

    return a->work_at < b->work_at;
}

int
schedule_work(struct work *work)
{
    spin_lock(&work_lock);
    list_insert_ordered(&work_list, &work->elem, work_less, NULL);
    spin_unlock(&work_lock);

    task_kick(worker_task);

    return 0;
}

int
schedule_work_at(struct work *work, uint32_t abs)
{
    if (abs < work_get_ticks())
        return 1;

    work->work_at = abs;

    return schedule_work(work);
}

int
schedule_work_delay(struct work *work, uint32_t delay)
{
    return schedule_work_at(work, work_get_ticks() + delay);
}

int
work_reschedule(uint32_t delay)
{
    panic_ifnot(current_task == worker_task);

    if (current_work->work_flags & WORK_FLAG_CANCELLED)
        return -1;

    schedule_work_delay(current_work, delay);

    return 0;
}

int
work_cancel(struct work *work)
{
    spin_lock(&work_lock);

    if (work == current_work) {
        /* if the currently running work is being cancelled, set a flag */
        work->work_flags |= WORK_FLAG_CANCELLED;
    } else if (work->work_flags & WORK_FLAG_QUEUED == 0) {
        return -1;
    } else
        list_remove(&work->elem);

    spin_unlock(&work_lock);

    return 0;
}

int
work_cancel_current(void)
{
    panic_ifnot(current_task == worker_task);

    return work_cancel(current_work);
}

struct work *
work_create(void (*f)(void *), void *aux)
{
    struct work *work = malloc(sizeof(*work));
    if (!work)
        return NULL;

    work->work_func = f;
    work->work_aux = aux;
    work->work_at = 0;
    work->work_flags = 0;

    return work;
}

void
work_destroy(struct work *work)
{
    free(work);
}

void
do_work(struct work *work)
{
    current_work = work;

    work->work_func(work->work_aux);
    
    work_destroy(work);
}

void
worker_thread(void)
{
    struct work *work;

    while (1) {
        if (list_empty(&work_list)) {
            sched_yield();
            continue;
        }

        spin_lock(&work_lock);
        work = list_entry(list_pop_front(&work_list), struct work, elem);
        if (work->work_at >= work_get_ticks()) {
            list_push_front(&work_list, &work->elem);
            spin_unlock(&work_lock);
            sleep(work->work_at - work_get_ticks());
            continue;
        }
        spin_unlock(&work_lock);

        do_work(work);
    }
}

int
work_init(void)
{
    spin_lock_init(&work_lock);
    list_init(&work_list);

    worker_task = create_kernel_task(worker_thread);
    sched_add_rq(worker_task);

    printk("kworker initialized\n");

    return 0;
}
