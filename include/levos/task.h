#ifndef __LEVOS_TASK_H
#define __LEVOS_TASK_H

#include <levos/types.h>
#include <levos/compiler.h>
#include <levos/kernel.h>
#include <levos/page.h>
#include <levos/fs.h>
#include <levos/signal.h>

typedef int pid_t;

struct bin_state
{
    uint32_t entry;
    void *switch_stack;
};

struct task
{
#define LEVOS_TASK_MAGIC 0xC0FFEEEE
    int task_magic;

    pid_t pid;

#define TASK_UNKNOWN   0   /* BUG */
#define TASK_NEW       1   /* was just created, needs stack */
#define TASK_RUNNING   2   /* currently running */
#define TASK_PREEMPTED 3   /* was preempted by another task */
#define TASK_BLOCKED   4   /* blocked on some condition */
#define TASK_ZOMBIE    5   /* died but parent is alive */
#define TASK_DYING     6   /* should get destroyed this tick */
#define TASK_DEAD      7   /* BUG */
    int state;

#define FD_MAX 16
    struct file *file_table[FD_MAX];

    int time_ran;
    int exit_code;

    struct bin_state bstate;

    struct task *parent;

    uint32_t *irq_stack_top;
    uint32_t *irq_stack_bot;

    struct pt_regs *new_stack;

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

struct task *create_user_task_fork(void (*)(void));

void sched_tick(struct pt_regs *);
void sched_init(void);
void sched_yield(void);

extern struct task *current_task;

#endif /* __LEVOS_TASK_H */
