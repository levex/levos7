#include <levos/kernel.h>
#include <levos/task.h>
#include <levos/signal.h>
#include <levos/list.h>
#include <levos/palloc.h>

int
task_has_pending_signals(struct task *task)
{
    /* wait until the current signal finishes */
    if (task->signal.sig_state == SIG_STATE_HANDLING)
        return 0;

    return !(list_empty(&task->signal.pending_signals));
}

int
signal_processing(struct task *task)
{
    return task->signal.sig_state != SIG_STATE_NULL;
}

int
signal_is_signum_valid(int signum)
{
    return MIN_SIG <= signum && signum <= MAX_SIG;
}

void
do_fatal_signal(struct task *task, int signal)
{
    //printk("task pid=%d caught fatal signal %s\n", task->pid, signal_to_string(signal));

    task->owner->status = TASK_DEATH_BY_SIGNAL;
    task->owner->exit_code = signal;

    free(task->signal.current_signal);
    task->signal.current_signal = NULL;

    task_exit(task);
    if (task == current_task)
        sched_yield();

    return;
}

void
__send_signal(struct task *task, int signal)
{
    //printk("%s: to %d from %d, sig: %s\n", __func__, task->pid, current_task->pid, signal_to_string(signal));
    struct signal *sig = malloc(sizeof(*sig));
    if (!sig) {
        printk("CRITICAL: ran out of memory while delivering a signal\n");
        task_exit(task);
    }
    memset(sig, 0, sizeof(*sig));
    sig->id = signal;
    /* XXX: later, we will push data here */
    //sig->data = task->sys_regs;
    //printk("%s 0x%x\n", __func__, sig->data);
    //dump_registers(sig->data);
    
    if (task->state == TASK_BLOCKED &&
            task->owner->status == TASK_SUSPENDED &&
            signal == SIGCONT) {
        //printk("SIGCONT is now queued in %d\n", task->pid);
        task->flags &= ~(TFLAG_WAITED);
        task->owner->status = 0;
        task->state = TASK_PREEMPTED;
        return;

        struct list_elem *elem;
        printk("tasks in run queue:\n");
        //spin_lock(&all_tasks_lock);
        list_foreach_raw(__ALL_TASKS_PTR, elem) {
            struct task *task = list_entry(elem, struct task, all_elem);
            __dump_task(task);
        }
        //spin_unlock(&all_tasks_lock);
        printk("zombie processes:\n");
        extern struct list zombie_processes;
        list_foreach_raw(&zombie_processes, elem) {
            struct task *task = list_entry(elem, struct task, all_elem);
            __dump_process(task);
        }
    }

    if (task->state == TASK_SLEEPING) {
        task->flags |= TFLAG_INTERRUPTED;
        task->state = TASK_PREEMPTED;
    }

    list_push_back(&task->signal.pending_signals, &sig->elem);
}

void
send_signal(struct task *task, int signal)
{
    if (task == NULL)
        return;

    __send_signal(task, signal);

    if (task == current_task)
        reschedule_to(task);
}

void
send_signal_group(pid_t pgid, int signal)
{
    struct task *task = NULL;
    struct list_elem *elem;

    list_foreach_raw(__ALL_TASKS_PTR, elem) {
        task = list_entry(elem, struct task, all_elem);
        if (task->pid == pgid || task->pgid == pgid) {
            //printk("sent a %s sig to pgid %d pid %d\n", signal_to_string(signal),
                    //task->pgid, task->pid);
            __send_signal(task, signal); 
        }
    }

    /* if our PG has been signalled, we need to reschedule to process
     * the signal
     */
    if (current_task->pgid == pgid)
        reschedule_to(current_task);
}

void
send_signal_group_of(struct task *task, int signal)
{
    send_signal_group(task->pgid, signal);
}

/* XXX: fixup with send_signal_group, they are the same */
void send_signal_session(pid_t sid, int signal)
{
    struct task *task = NULL;
    struct list_elem *elem;
    list_foreach_raw(__ALL_TASKS_PTR, elem) {
        task = list_entry(elem, struct task, all_elem);
        if (task->sid == sid) {
            __send_signal(task, signal); 
        }
    }

    /* if our session has been signalled, we need to reschedule to process
     * the signal
     */
    if (current_task->sid == sid)
        reschedule_to(current_task);

}

void
send_signal_session_of(struct task *task, int signal)
{
    send_signal_session(task->sid, signal);
}

void
default_sigaction(struct task *task, struct signal *sig)
{
    if (sig->id == SIGCONT) {
        free(sig);
        task->signal.current_signal = NULL;
        task->signal.sig_state = SIG_STATE_NULL;
        return;
    }

    /* default action of SIGCHLD is to ignore */
    if (sig->id == SIGCHLD) {
        free(sig);
        task->signal.current_signal = NULL;
        task->signal.sig_state = SIG_STATE_NULL;
        return;
    }

    if (sig->id == SIGSTOP ||
            sig->id == SIGTSTP ||
            sig->id == SIGTTOU ||
            sig->id == SIGTTIN) {
        int sigid = sig->id;
        free(sig);
        task->signal.current_signal = NULL;
        task->signal.sig_state = SIG_STATE_NULL;
        task_suspend(task, sigid);
        return;
    }

    do_fatal_signal(task, sig->id);
}

void
signal_register_handler(struct task *task, int signum, sighandler_t handler)
{
    struct signal_struct *sig = &task->signal;

    /* quietly fail */
    if (signum == SIGKILL || signum == SIGSTOP)
        return;

    //printk("registering signal handler for %s(%d) to 0x%x\n", signal_to_string(signum), signum,
            //handler);
    sig->signal_handlers[signum] = handler;
}

sighandler_t
signal_get_disp(struct task *task, int signum)
{
    struct signal_struct *sig = &task->signal;

    return sig->signal_handlers[signum];
}

void
ret_from_signal(void)
{
    struct task *task = current_task;
    struct signal_struct *sig = &task->signal;

    //dump_registers(&sig->current_signal->context);

    //printk("context of %s is at 0x%x\n", signal_to_string(sig->current_signal->id),
            //&sig->current_signal->context);
    //dump_registers(&sig->current_signal->context);
    
    panic_ifnot(sig->current_signal);
    //printk("RETURNING FROM SIGNAL %s &task 0x%x %x \n", signal_to_string(sig->current_signal->id), &task, task->regs);

    //task->regs = &sig->current_signal->context;
    memcpy(task->regs, &sig->current_signal->context, sizeof(struct pt_regs));

    /* if the task has been processing a syscall, then we can't continue
     * executing the syscall. fixup the system call return code to EINTR and
     * pass the control and decision to userspace
     */
    if (task->flags & TFLAG_PROCESSING_SYSCALL) {
        task->regs->eax = -EINTR;
        task->flags &= ~TFLAG_PROCESSING_SYSCALL;
    }

    free(sig->current_signal);
    sig->current_signal = NULL;
    sig->sig_state = SIG_STATE_NULL;
    //printk("finished ret_from_signal\n");
    task->flags |= TFLAG_NO_SIGNAL;
    reschedule_to(task);
}

/* XXX: when VMAs are implemented, swap the VMA instead of the pages */
void
do_signal_handle(struct task *task, int signum, sighandler_t handler)
{
    struct signal_struct *sig = &task->signal;
    page_t *pgptr;
    uint32_t *stack_ptr;

    //printk("about to handle signal %s, 0x%x\n", signal_to_string(signum), &pgptr);
    //printk("signal's context: \n");
    //dump_registers(sig->current_signal->data);

    /* start handling a signal */
    sig->sig_state = SIG_STATE_HANDLING;

    /* save the bottom of the stack */
    sig->unused_stack_bot = (void *) VIRT_BASE - (0x1000 * 11);

    /* first, save the old stack pointer */
    sig->unused_stack = (void *) task->sys_regs;

    /* save the context */
    memcpy(&sig->current_signal->context, task->sys_regs, sizeof(struct pt_regs));
    //printk("Wooohoo\n");
    //dump_registers(task->sys_regs);

    /* map the signal stack */
    map_page(task->mm, sig->stack_phys_page, (uint32_t) sig->unused_stack_bot, 1);
    __flush_tlb();

    //pagedir_t pgd = activate_pgd_save(task->mm);

    //printk("signal stack bottom: 0x%x\n", sig->unused_stack_bot);

    /* zero the stack */
    memset(sig->unused_stack_bot, 0, 4096);

    /* setup the stack frame */
    stack_ptr = (uint32_t *)(sig->unused_stack_bot + 0x1000);

    //printk("signal stack begins at 0x%x\n", stack_ptr);

    stack_ptr -= 16;

    //printk("stack_ptr is 0x%x\n", stack_ptr);

    /* push the argument */
    *--stack_ptr = signum;
    //printk("rofl im dead\n");

    /* push the return address */
    *--stack_ptr = (uint32_t) ret_from_signal;

    uint32_t tmp = (uint32_t) stack_ptr;
    *--stack_ptr = 0x23; /* ss */
    *--stack_ptr = tmp; /* esp */
    *--stack_ptr = 0x202; /* eflags */
    *--stack_ptr = 0x1B; /* cs  */
    *--stack_ptr = (uint32_t) handler; /* eip */
    *--stack_ptr = 0x00; /* framepointer */
    *--stack_ptr = 0x00; /* error_code */
    *--stack_ptr = 0x00; /* vec_no */
    *--stack_ptr = 0x23; /* ds  */
    *--stack_ptr = 0x23; /* es  */
    *--stack_ptr = 0x23; /* fs  */
    *--stack_ptr = 0x23; /* gs  */
    /* pushad */
    tmp = (uint32_t) stack_ptr;
    *--stack_ptr = 0;    /* eax */
    *--stack_ptr = 0;    /* ecx */
    *--stack_ptr = 0;    /* edx */
    *--stack_ptr = 0;    /* ebx */
    *--stack_ptr = tmp;  /* esp_dummy */
    *--stack_ptr = tmp;    /* ebp */
    *--stack_ptr = 0;    /* esi */
    *--stack_ptr = 0;    /* edi */

    //printk("is it still the same? 0x%x\n", sig->current_signal->data);
    //dump_registers(sig->current_signal->data);

    task->regs = (void *) stack_ptr;
}

int
signal_is_blocked(struct signal_struct *sig, int signum)
{
    return sig->sig_mask & (1 << signum);
}

void
signal_set_mask(struct task *task, uint32_t mask)
{
    task->signal.sig_mask = mask;
}

void
signal_get_mask(struct task *task)
{
    return task->signal.sig_mask;
}

int
signal_should_resched(struct task *task)
{
    struct signal_struct *sig = &task->signal;

    /* if there are no pending signals, then don't reschedule */
    if (!task_has_pending_signals(task))
        return 0;

    if (sig->sig_flags & SFLAG_RESCHED) {
        sig->sig_flags &= ~SFLAG_RESCHED;
        return 1;
    }

    return 0;
}

void
signal_handle(struct task *task)
{
    struct signal_struct *sig = &task->signal;
    struct signal *signal;
    struct list backup_list;
    int signum;

    //printk("pid %d: looking for a signal to process, there are %d pending\n",
            //task->pid, list_size(&sig->pending_signals));
    spin_lock(&sig->sig_lock);

    /* if in the meantime we started processing, return */
    if (signal_processing(task)) {
        //printk("pid %d: oops a signal is already processing\n", task->pid);
        spin_unlock(&sig->sig_lock);
        return;
    }

    //printk("pid %d: finding a signal to process\n", task->pid);

    list_init(&backup_list);
retry:
    signal = list_entry(list_pop_front(&sig->pending_signals), struct signal, elem);
    signum = signal->id;


    /* check if the signal is blocked */
    if (signal_is_blocked(sig, signum)) {
        /* save this signal */
        list_push_back(&backup_list, &signal->elem);

        /* if there are no more pending signals, restore the list,
         * and return to scheduling
         */
        if (list_empty(&sig->pending_signals)) {
            while (!list_empty(&backup_list)) {
                signal = list_entry(list_pop_front(&backup_list), struct signal, elem);
                list_push_back(&sig->pending_signals, &signal->elem);
            }

            spin_unlock(&sig->sig_lock);
            return;
        } else goto retry;
    } else {
        struct signal *old_sig = signal;

        /* check if we pushed any signal away */
        while (!list_empty(&backup_list)) {
            signal = list_entry(list_pop_front(&backup_list), struct signal, elem);
            list_push_back(&sig->pending_signals, &signal->elem);
        }

        signal = old_sig;
    }

    sig->sig_state = SIG_STATE_HANDLING;
    sig->current_signal = signal;

    //printk("handling signal in pid %d: %s, action %s\n",
            //task->pid, signal_to_string(signum), sig->signal_handlers[signum]
             //== SIG_DFL ? "default action" : sig->signal_handlers[signum] == SIG_IGN ?
                //"ignore" : "handler");

    //dump_registers(task->regs);

    if (sig->signal_handlers[signum] == SIG_DFL) {
        default_sigaction(task, signal);
    } else if (sig->signal_handlers[signum] == SIG_IGN) {
        sig->sig_state = SIG_STATE_NULL;
        sig->current_signal = NULL;
        free(signal);
        spin_unlock(&sig->sig_lock);
        return;
    } else
        do_signal_handle(task, signum, sig->signal_handlers[signum]);

    spin_unlock(&sig->sig_lock);
}

void
copy_signals(struct task *dst, struct task *src)
{
    struct signal_struct *sdst = &dst->signal;
    struct signal_struct *ssrc = &src->signal;

    for (int i = MIN_SIG; i < MAX_SIG; i ++)
        sdst->signal_handlers[i] = ssrc->signal_handlers[i];

    /* copy the sigmask */
    sdst->sig_mask = ssrc->sig_mask;
}

void
signal_reset(struct task *task)
{
    struct signal_struct *sig = &task->signal;
    int i;

    /* set default actions */
    for (i = MIN_SIG; i < MAX_SIG; i ++)
        sig->signal_handlers[i] = SIG_DFL;

    /* SIGCHLD is ignored by default */
    //sig->signal_handlers[SIGCHLD] = SIG_IGN;
}

void
signal_reset_for_execve(struct task *task)
{
    struct signal_struct *sig = &task->signal;
    int i;

    /* set default actions */
    for (i = MIN_SIG; i < MAX_SIG; i ++) {
        if (sig->signal_handlers[i] == SIG_IGN) {
            /* leave as ignored */
            sig->signal_handlers[i] = SIG_IGN;
        } else {
            /* all signals that were not ignored are reset to their default action */
            sig->signal_handlers[i] = SIG_DFL;
        }

    }

    /* SIGCHLD is ignored by default */
    //sig->signal_handlers[SIGCHLD] = SIG_IGN;
}

void
signal_init(struct task *task)
{
    struct signal_struct *sig = &task->signal;
    int i;

    memset(sig, 0, sizeof(*sig));

    /* initalize pending signals */
    list_init(&sig->pending_signals);

    /* reset the signal mask */
    sig->sig_mask = 0;

    /* reset signal handlers */
    signal_reset(task);

    spin_lock_init(&sig->sig_lock);

    /* allocate signal stack */
    sig->stack_phys_page = palloc_get_page();

    //printk("pid %d has signals setup, phypage: 0x%x\n", task->pid, sig->stack_phys_page);
}
