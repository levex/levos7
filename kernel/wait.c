#include <levos/kernel.h>
#include <levos/task.h>

/*
 * wait_ev_create - create a wait event
 *
 * @who - initiating process ("something happened to me")
 * @what - what happened to @who
 * @extra - any extra details (exit code, signal, etc.)
 */
struct wait_ev *
wait_ev_create(struct process *who, int what, int extra)
{
    struct wait_ev *ev = malloc(sizeof(*ev));
    if (!ev)
        return NULL;

    ev->wev_what = what;
    ev->wev_proc = who;
    ev->wev_extra = extra;

    return ev;
}

/*
 * wait_ev_queue - queue a wait event for processing later
 *                 in a task
 *
 *  @task - the task that will process this event later
 *  @wev - the wait event
 */
void
wait_ev_queue(struct task *task, struct wait_ev *wev)
{
    spin_lock(&task->wait_ev_lock);
    list_push_back(&task->wait_ev_list, &wev->wev_elem);
    spin_unlock(&task->wait_ev_lock);
}

/*
 * wait_ev_destroy - destroy a wait event
 * 
 * @ev - the wait_ev to destroy
 */
void
wait_ev_destroy(struct wait_ev *ev)
{
    free(ev);
}
