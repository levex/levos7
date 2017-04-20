#ifndef __LEVOS_TASK_H
#define __LEVOS_TASK_H

#include <levos/types.h>
#include <levos/compiler.h>
#include <levos/kernel.h>
#include <levos/page.h>
#include <levos/fs.h>
#include <levos/signal.h>
#include <levos/list.h>


#define WAIT_CODE(info, code) ((int)((uint16_t)(((info) << 8 | (code)))))
#define WAIT_EXIT(a) WAIT_CODE(a, 0)

typedef int pid_t;

struct bin_state
{
    uint32_t entry;
    void *switch_stack;
    char **argvp;
    char **envp;
};

struct task;

struct process
{
    pid_t pid;
    pid_t ppid;
    struct task *task;
    int exit_code;
    int status;

    struct list waiters;

    struct list_elem elem;
};

struct task
{
#define LEVOS_TASK_MAGIC 0xC0FFEEEE
    int task_magic;

    pid_t pid;

#define TFLAG_WAITED   (1 << 0)
    int flags;

#define TASK_UNKNOWN   0   /* BUG */
#define TASK_NEW       1   /* was just created, needs stack */
#define TASK_RUNNING   2   /* currently running */
#define TASK_PREEMPTED 3   /* was preempted by another task */
#define TASK_BLOCKED   4   /* blocked on some condition */
#define TASK_ZOMBIE    5   /* died but parent is alive */
#define TASK_DYING     6   /* should get destroyed this tick */
#define TASK_DEAD      7   /* BUG */
#define TASK_SLEEPING  8   /* sleeping */
    int state;

#define FD_MAX 16
    struct file *file_table[FD_MAX];

    int time_ran;
    int exit_code;
    struct process *owner;

    struct bin_state bstate;

    struct signal_struct signal;

    /*
     * uh, not sure we need this lock, since only the
     * process will modify its children
     */
    //spinlock_t              children_lock;
    struct list      children_list;
    struct list_elem children_elem;
    struct task *parent;

    struct list_elem wait_elem;

    uint32_t *irq_stack_top;
    uint32_t *irq_stack_bot;

    struct pt_regs *new_stack;

    uint32_t wake_time;

    pagedir_t mm;

    /* regs (if interrupted) */
    struct pt_regs *regs;
    struct pt_regs *sys_regs;
};

inline int task_runnable(struct task *t)
{
    return t->state == TASK_NEW ||
            t->state == TASK_PREEMPTED;
}
int task_do_wait(struct task *, struct process *);
void task_exit(struct task *t);

struct task *create_user_task_fork(void (*)(void));
struct task *create_kernel_task(void (*)(void));
struct task *get_task_for_pid(pid_t);


void reschedule_to(struct task *);

void sched_tick(struct pt_regs *);
void sched_init(void);
void sched_yield(void);
void sched_add_rq(struct task *);
void sched_add_child(struct task *, struct task *);
struct process *sched_get_child(struct task *, pid_t);

void task_sleep(struct task *, uint32_t);
void task_kick(struct task *);
void sleep(uint32_t);

void setup_filetable(struct task *);

extern struct task *current_task;

#endif /* __LEVOS_TASK_H */
