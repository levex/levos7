#include <levos/kernel.h>
#include <levos/wait.h>
#include <levos/task.h>

void
wait_queue_init(wait_queue_t *wq)
{
    list_init(&wq->wq_waiters);
}

void
wait_task_on(struct task *task, wait_queue_t *wq)
{
    preempt_disable();
    list_push_back(&wq->wq_waiters, &task->wait_elem);
    preempt_enable();
}

void
wait_on(wait_queue_t *wq)
{
    wait_task_on(current_task, wq);
}

int
wait_queue_num_waiters(wait_queue_t *wq)
{
    return wq->wq_num;
}

struct task *
wait_wake_up_one(wait_queue_t *wq)
{
    struct list_elem *elem = list_pop_front(&wq->wq_waiters);
    struct task *task = list_entry(elem, struct task, wait_elem);

    task_unblock(task);

    return task;
}

void
wait_wake_up(wait_queue_t *wq)
{
    int i = wait_queue_num_waiters(wq);
    while (i --)
        wait_wake_up_one(wq);
}
