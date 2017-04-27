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
    sig->data = task->sys_regs;
    //printk("%s 0x%x\n", __func__, sig->data);
    //dump_registers(sig->data);


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
        if (task->pgid == pgid) {
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
default_sigaction(struct task *task, int signum)
{
    if (signum == SIGCONT) {
        /* XXX FIXME: this is not implemented yet properly */
        task->state = TASK_PREEMPTED;
        /* XXX: fallthrough until this is clarified and fixed */
    }

    if (signum == SIGSTOP ||
            signum == SIGTSTP ||
            signum == SIGTTOU ||
            signum == SIGTTIN) {
        /* XXX FIXME: not correct */
        task->state = TASK_BLOCKED;
        /* XXX: fallthrough until this is clarified and fixed */
    }

    do_fatal_signal(task, signum);
}

void
signal_register_handler(struct task *task, int signum, sighandler_t handler)
{
    struct signal_struct *sig = &task->signal;

    /* quietly fail */
    if (signum == SIGKILL || signum == SIGSTOP)
        return;

    //printk("registering signal handler for %s to 0x%x\n", signal_to_string(signum),
            //handler);
    sig->signal_handlers[signum] = handler;
}

void
ret_from_signal(void)
{
    struct task *task = current_task;
    struct signal_struct *sig = &task->signal;

    //printk("RETURNING FROM SIGNAL &task 0x%x %x \n", &task, task->regs);
    //dump_registers(&sig->current_signal->context);

    //task->regs = &sig->current_signal->context;
    memcpy(task->regs, &sig->current_signal->context, sizeof(struct pt_regs));
    sig->sig_state = SIG_STATE_NULL;
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
    sig->unused_stack_bot = (void *) VIRT_BASE - 0x2000;

    /* first, save the old stack pointer */
    sig->unused_stack = (void *) task->sys_regs;

    /* save the context */
    memcpy(&sig->current_signal->context, task->sys_regs, sizeof(struct pt_regs));

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

void
signal_handle(struct task *task)
{
    struct signal_struct *sig = &task->signal;
    struct signal *signal;
    int signum;

    signal = list_entry(list_pop_front(&sig->pending_signals), struct signal, elem);
    signum = signal->id;

    sig->current_signal = signal;

    //printk("handling signal in pid %d: %s, action %s\n",
            //task->pid, signal_to_string(signum), sig->signal_handlers[signum]
             //== SIG_DFL ? "default action" : sig->signal_handlers[signum] == SIG_IGN ?
                //"ignore" : "handler");

    if (sig->signal_handlers[signum] == SIG_DFL)
        default_sigaction(task, signum);
    else if (sig->signal_handlers[signum] == SIG_IGN)
        return;
    else
        do_signal_handle(task, signum, sig->signal_handlers[signum]);
}

void
signal_init(struct task *task)
{
    struct signal_struct *sig = &task->signal;
    int i;

    memset(sig, 0, sizeof(*sig));

    /* initalize pending signals */
    list_init(&sig->pending_signals);

    /* set default actions */
    for (i = MIN_SIG; i <= MAX_SIG; i ++)
        sig->signal_handlers[i] = SIG_DFL;

    /* SIGCHLD is ignored by default */
    sig->signal_handlers[SIGCHLD] = SIG_IGN;

    /* allocate signal stack */
    sig->stack_phys_page = palloc_get_page();

    //printk("pid %d has signals setup, phypage: 0x%x\n", task->pid, sig->stack_phys_page);
}
