#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/task.h>
#include <levos/x86.h> /* FIXME */
#include <levos/intr.h>
#include <levos/arch.h>
#include <levos/palloc.h>
#include <levos/page.h>
#include <levos/spinlock.h>
#include <levos/list.h>

#define TIME_SLICE 15

static pid_t last_pid = 0;
struct task *current_task;

static spinlock_t all_tasks_lock;
static struct list all_tasks;

struct list *__ALL_TASKS_PTR = &all_tasks;

//static int last_task;

int __task_need_cleanup;

struct list zombie_processes;

static int __preempt_enabled;

void __noreturn late_init(void);
void __noreturn __idle_thread(void);

void intr_yield(struct pt_regs *);
void sched_yield(void);
void reschedule(void);

void
preempt_enable(void)
{
    __preempt_enabled = 1;
}

void
preempt_disable(void)
{
    __preempt_enabled = 0;
}

inline int
task_is_kernel(struct task *t)
{
    return t->mm == 0;
}

pid_t
allocate_pid(void)
{
    return ++last_pid;
}

void __noreturn
sched_init(void)
{
    uint32_t kernel_stack, new_stack;

    printk("sched: init\n");

    list_init(&zombie_processes);
    list_init(&all_tasks);
    spin_lock_init(&all_tasks_lock);

    current_task = malloc(sizeof(*current_task));
    if (!current_task)
        panic("Kernel ran out of memory when starting threading\n");

    current_task->mm = 0;
    current_task->pid = 0;
    current_task->pgid = 0;
    current_task->ppid = 0;
    current_task->sid = 0;
    current_task->state = TASK_RUNNING;
    current_task->time_ran = 0;
    signal_init(current_task);
    vma_init(current_task);
    spin_lock_init(&current_task->vm_lock);
    list_init(&current_task->wait_ev_list);
    spin_lock_init(&current_task->wait_ev_lock);
    char *fxsave = na_malloc(512, 16);
    memset(fxsave, 0, 512);
    current_task->sse_save = fxsave;
    do_sse_save(current_task);

    //memset(all_tasks, 0, sizeof(struct task *) * 128);
    //all_tasks[0] = current_task;
    spin_lock(&all_tasks_lock);
    list_push_back(&all_tasks, &current_task->all_elem);
    spin_unlock(&all_tasks_lock);
    //last_task = 0;

    intr_register_user(0x2f, intr_yield);

    /* map a new stack */
    new_stack = (uint32_t) malloc(4096);
    if (!new_stack)
        panic("ENOMEM for new kernel stack\n");

    new_stack = (int)new_stack + 4096;

    /* switch stack */
    asm volatile ("movl %%esp, %0; movl %1, %%esp; movl %1, %%ebp"
            :"=r"(kernel_stack)
            :"r"(new_stack));

    //    printk("sched: kernel stack was: 0x%x\n", kernel_stack);

    /* start executing the process */
    __idle_thread();

    /* this is weird */
    panic("tasking screwed\n");
}

    struct process *
process_create(struct task *task)
{
    struct process *proc = malloc(sizeof(*proc));
    if (!proc)
        return NULL;

    memset(proc, 0, sizeof(*proc));

    proc->pid = task->pid;
    proc->task = task;
    list_init(&proc->waiters);
    return proc;
}

    void
process_destroy(struct process *proc)
{
    free(proc);
}

    void
process_add_waiter(struct process *proc, struct task *waiter)
{
    list_push_back(&proc->waiters, &waiter->wait_elem);
}

    int
__task_init(struct task *task)
{
    memset(task, 0, sizeof(*task));
    task->pid = allocate_pid();
    task->ppid = 0;
    task->pgid = 0;
    task->state = TASK_PREEMPTED;
    task->time_ran = 0;
    task->parent = NULL;
    task->exit_code = 0;
    task->mm = kernel_pgd;
    task->flags = 0;
    task->cwd = strdup("/");
    list_init(&task->children_list);
    list_init(&task->wait_ev_list);
    spin_lock_init(&task->wait_ev_lock);
    vma_init(task);
    spin_lock_init(&task->vm_lock);
    signal_init(task);
    task->owner = process_create(task);
    if (!task->owner) {
        //free_pid(task->pid); // TODO
        free(task);
        return -ENOMEM;
    }
    char *fxsave = na_malloc(512, 16);
    memset(fxsave, 0, 512);
    task->sse_save = fxsave;
    task->owner->status = 0;
    task->owner->exit_code = 0;

    return 0;
}

struct task *create_kernel_task(void (*func)(void))
{
    uint32_t *new_stack, tmp;
    struct task *task = malloc(sizeof(*task));
    int rc;

    if (!task)
        return NULL;

    rc = __task_init(task);
    if (rc)
        return NULL;

    task->comm = strdup("kthread");

    /* get a stack for the task, since this is a kernel thread
     * we can use malloc
     */
    new_stack = malloc(4096);
    if (!new_stack) {
        free(task);
        return NULL;
    }
    memset(new_stack, 0, 4096);

    new_stack = (uint32_t *)((int)new_stack + 4096);
    task->irq_stack_top = (uint32_t *) (((uint8_t *) malloc(0x1000)) + 0x1000);
    task->irq_stack_bot = (void *) ((uint32_t)task->irq_stack_top) - 0x1000;

    /* setup the stack */
    new_stack -= 16;
    *--new_stack = 0x10; /* ss */
    *--new_stack = 0; /* esp */
    *--new_stack = 0x202; /* eflags */
    *--new_stack = 0x08; /* cs  */
    *--new_stack = (uint32_t) func; /* eip */
    *--new_stack = 0x00; /* framepointer */
    *--new_stack = 0x00; /* error_code */
    *--new_stack = 0x00; /* vec_no */
    *--new_stack = 0x10; /* ds  */
    *--new_stack = 0x10; /* es  */
    *--new_stack = 0x10; /* fs  */
    *--new_stack = 0x10; /* gs  */
    /* pushad */
    tmp = (uint32_t) new_stack;
    *--new_stack = 0;    /* eax */
    *--new_stack = 0;    /* ecx */
    *--new_stack = 0;    /* edx */
    *--new_stack = 0;    /* ebx */
    *--new_stack = tmp;  /* esp_dummy */
    *--new_stack = 0;    /* ebp */
    *--new_stack = 0;    /* esi */
    *--new_stack = 0;    /* edi */

    task->regs = (void *) new_stack;
    task->new_stack = (void *) new_stack;
    return task;
}

void
task_block_noresched(struct task *task)
{
    task->state = TASK_BLOCKED;
    //printk("%s: pid %d (%s)\n", __func__, task->pid, task->comm);
}

void
task_block(struct task *task)
{
    task_block_noresched(task);
    if (task == current_task)
        sched_yield();
}

void
task_suspend(struct task *task, int sig)
{
    task->owner->status = TASK_SUSPENDED;
    task->owner->exit_code = sig;
    task_unblock_waiters(task);
    task_block_noresched(task);
}

void
task_continue(struct task *task)
{
    /* FIXME: is this correct semantic ? */
    task->owner->status = 0;
    //task->owner->exit_code = SIGCONT;
    //task_unblock_waiters(task);
    task_unblock(task);
}

void
task_sleep(struct task *task, uint32_t ticks)
{
    task->wake_time = ticks;
    task->state = TASK_SLEEPING;
}

    void
task_kick(struct task *task)
{
    //panic_ifnot(task->state == TASK_SLEEPING);
    task->wake_time = 0;
    task->state = TASK_PREEMPTED;
    //printk("sched: kicked task %d\n", task->pid);
}

    void
sleep(uint32_t ticks)
{
    extern uint32_t __pit_ticks;
    task_sleep(current_task, __pit_ticks + 1 + ticks);
    sched_yield();
}

    void
task_unblock(struct task *task)
{
    panic_ifnot(task != current_task);
    task->state = TASK_PREEMPTED;
}

    char *
get_comm_for_pid(pid_t pid)
{
    if (pid > 0) {
        struct task *task = get_task_for_pid(pid);

        if (task)
            return task->comm;
        else return "non_existent";
    }

    return "group_pid";
}

    void
setup_filetable(struct task *task)
{
    /* for stdin, we currently just pass through to the serial,
     * but in the future we really should implement ttys and
     * give the init process a different stdin
     *
     * FIXME
     */

    extern struct file serial_base_file;

    /* stdin */
    task->file_table[0] = dup_file(&serial_base_file);
    /* stdout */
    task->file_table[1] = dup_file(&serial_base_file);
    /* stderr */
    task->file_table[2] = dup_file(&serial_base_file);
}

    void
copy_filetable(struct task *dst, struct task *src)
{
    int i;

    /* XXX: setup_filetable has dupped a few files, get rid of them */
    free(dst->file_table[0]->respath);
    free(dst->file_table[1]->respath);
    free(dst->file_table[2]->respath);

    free(dst->file_table[0]);
    free(dst->file_table[1]);
    free(dst->file_table[2]);

    /* actually copy the table, increasing refc */
    for (i = 0; i < FD_MAX; i ++) {
        dst->file_table[i] = src->file_table[i];
        if (dst->file_table[i])
            vfs_inc_refc(dst->file_table[i]);
    }
}

    void
close_filetable(struct task *task)
{
    int i;

    for (i = 0; i < FD_MAX; i ++) {
        struct file *filp = task->file_table[i];

        if (filp)
            vfs_close(filp);

        task->file_table[i] = NULL;
    }
}

void
__dump_task(struct task *task)
{
    printk("task %d(%s) state: %s\n",
            task->pid, task->comm,
            task->state == TASK_UNKNOWN ? "unknown" :
            task->state == TASK_DEAD ? "dead" :
            task->state == TASK_NEW ? "new" :
            task->state == TASK_RUNNING ? "running" :
            task->state == TASK_PREEMPTED ? "preempted" :
            task->state == TASK_BLOCKED ? "blocked" :
            task->state == TASK_ZOMBIE ? "zombie" :
            task->state == TASK_DYING ? "dying" :
            "sleeping");
}

void
__dump_process(struct process *proc)
{
    printk("process %d exited with code %d from %s\n",
            proc->pid, proc->exit_code,
            proc->status == TASK_EXITED ? "exit(2)" : "signal");
}

void
task_unblock_waiters(struct task *t)
{
    /* check if there are waiters */
    if (!list_empty(&t->owner->waiters)) {
        struct list_elem *elem;
        /* wake them up */
        list_foreach_raw(&t->owner->waiters, elem) {
            struct task *waiter = list_entry(elem, struct task, wait_elem);

            //printk("%s: pid %d(%s) unblocked by pid %d(%s)\n",
                //__func__, waiter->pid, waiter->comm,
                //t->pid, t->comm);
            task_unblock(waiter);
       }
    }
}

void
task_exit(struct task *t)
{
    /* TODO: preliminary cleanup, but don't get rid of thread */
    t->state = TASK_ZOMBIE;
    if (t->pid == 1)
        panic("Attempting to exit from init\n");
    if (t->pid == 0)
        panic("Kernel bug: Attempting to kill idle/swapper\n");

#if 0
    if (t->pid == 10) {
        struct list_elem *elem;

        printk("tasks in run queue:\n");
        //spin_lock(&all_tasks_lock);
        list_foreach_raw(&all_tasks, elem) {
            struct task *task = list_entry(elem, struct task, all_elem);
            __dump_task(task);
        }
        //spin_unlock(&all_tasks_lock);
        printk("zombie processes:\n");
        list_foreach_raw(&zombie_processes, elem) {
            struct task *task = list_entry(elem, struct task, all_elem);
            __dump_process(task);
        }

    }
#endif

    //printk("TASK EXIT for %d\n", t->pid);
    
    /* queue a SIGCHLD to the parent */
    //printk("would send signal SIGCHLD to %d from %d\n", t->ppid, t->pid);
    send_signal(get_task_for_pid(t->ppid), SIGCHLD);

    /* flush the controlling terminal */
    if (t->ctty)
        tty_flush_output(t->ctty);

    /* check if there are waiters */
    task_unblock_waiters(t);

    if (t->flags & TFLAG_WAITED)
        process_destroy(t->owner);
    else {
        t->owner->task = NULL;
        list_push_back(&zombie_processes, &t->owner->elem);
    }

    free(t->comm);
    na_free(16, t->sse_save);
    free(t->bstate.switch_stack);
    close_filetable(t);
    list_remove(&t->children_elem);
    free(t->cwd);
    free(t->irq_stack_bot);
    list_remove(&t->all_elem);
    vma_unload_all(t);
    activate_pgd(kernel_pgd);
    mm_destroy(t->mm);
    free(t);
}

struct task *create_user_task_withmm(int do_stack, pagedir_t mm, void (*func)(void))
{
    uint32_t *new_stack, p_new_stack, tmp;
    struct task *task = malloc(sizeof(*task));
    int rc;

    if (!task)
        return NULL;

    rc = __task_init(task);
    if (rc)
        return NULL;

    task->mm = mm;

    task->irq_stack_top = (uint32_t *) (((uint8_t *) malloc(0x1000)) + 0x1000);
    task->irq_stack_bot = (void *) ((uint32_t)task->irq_stack_top) - 0x1000;

    if (!do_stack) {
        setup_filetable(task);
        return task;
    }

    /* get a stack for the task, since this is a kernel thread
     * we can use malloc
     */
    p_new_stack = palloc_get_page();
    if (!p_new_stack) {
        free(task);
        return NULL;
    }

    map_page(task->mm, p_new_stack, VIRT_BASE - 4096, 1);
    map_page_curr(p_new_stack, 0xD0000000 - 4096, 1);
    new_stack = (uint32_t *) (0xD0000000 - 4096);
    memset(new_stack, 0, 4096);

    new_stack = (uint32_t *)((int)new_stack + 4096);

    /* setup the stack */
    tmp = (uint32_t) new_stack;
    *--new_stack = 0x23; /* ss */
    *--new_stack = tmp; /* esp */
    *--new_stack = 0x202; /* eflags */
    *--new_stack = 0x1B; /* cs  */
    *--new_stack = (uint32_t) func; /* eip */
    *--new_stack = 0x00; /* framepointer */
    *--new_stack = 0x00; /* error_code */
    *--new_stack = 0x00; /* vec_no */
    *--new_stack = 0x23; /* ds  */
    *--new_stack = 0x23; /* es  */
    *--new_stack = 0x23; /* fs  */
    *--new_stack = 0x23; /* gs  */
    /* pushad */
    tmp = (uint32_t) new_stack;
    *--new_stack = 0;    /* eax */
    *--new_stack = 0;    /* ecx */
    *--new_stack = 0;    /* edx */
    *--new_stack = 0;    /* ebx */
    *--new_stack = tmp;  /* esp_dummy */
    *--new_stack = tmp;    /* ebp */
    *--new_stack = 0;    /* esi */
    *--new_stack = 0;    /* edi */

    task->regs = (void *) (VIRT_BASE - sizeof(struct pt_regs));
    ((struct pt_regs *) new_stack)->esp = task->regs;

    task->new_stack = (struct pt_regs *) new_stack;

    setup_filetable(task);
    return task;
}

void
sched_add_child(struct task *parent, struct task *child)
{
    list_push_back(&parent->children_list, &child->children_elem);
    child->parent = parent;
    child->owner->ppid = parent->pid;
    child->ppid = parent->pid;
    child->sid = parent->sid;
    child->pgid = parent->pgid;
}

struct process *
sched_get_child(struct task *parent, pid_t pid)
{
    struct list_elem *elem;

    /* first, check alive children */
    list_foreach_raw(&parent->children_list, elem) {
        struct task *task = list_entry(elem, struct task, children_elem);

        if (task->pid == pid)
            return task->owner;
    }

    /* no live children with pid, check zombie tasks */
    list_foreach_raw(&zombie_processes, elem) {
        struct process *proc = list_entry(elem, struct process, elem);

        if (proc->ppid == parent->pid && proc->pid == pid)
            return proc;
    }

    /* nope */
    return NULL;
}

struct process *
sched_get_any_child(struct task *parent)
{
    struct list_elem *elem;

    /* first, check alive children */
    list_foreach_raw(&parent->children_list, elem) {
        struct task *task = list_entry(elem, struct task, children_elem);

        if (task->ppid == parent->pid)
            return task->owner;
    }

    /* no live children with pid, check zombie tasks */
    list_foreach_raw(&zombie_processes, elem) {
        struct process *proc = list_entry(elem, struct process, elem);

        if (proc->ppid == parent->pid);
            return proc;
    }

    /* nope */
    return NULL;
}


int
task_do_wait(struct task *waiter, struct process *waited)
{
    panic_ifnot(waiter == current_task);
    int ret;

    process_add_waiter(waited, waiter);

    task_block(waiter);

    return __task_do_wait(waiter, waited);
}

int
__task_do_wait(struct task *waiter, struct process *waited)
{
    int ret;

    /* we've been woken up! */

    if (waited->task)
        waited->task->flags |= TFLAG_WAITED;

    /* XXX: process_destroy should be called here? */

    /* figure out what happened */
    if (waited->task == NULL) {
        if (waited->status == TASK_EXITED) {
            ret = WAIT_EXIT((uint8_t) waited->exit_code);
            //printk("the process has exited with exit code %d\n", waited->exit_code);
            //process_destroy(waited);
            return ret;
        } else if (waited->status == TASK_DEATH_BY_SIGNAL) {
            //printk("the process has been killed by a signal %d\n", waited->exit_code);
            ret = WAIT_SIG((uint8_t) waited->exit_code);
            //process_destroy(waited);
            return ret;
        }
    } 

    if (waited->status == TASK_SUSPENDED) {
        printk("a task that is waited on has been suspended\n");
        ret = WAIT_STOPPED((uint8_t) waited->exit_code);
        return ret;
    } else panic("invalid wait situation\n");

    return 0xffffffff;
}

struct task *
get_task_for_pid(pid_t pid)
{
    int i;
    struct list_elem *elem;
    struct task *ret = NULL;

    /*for (i = 0; i < 128; i ++)
        if (all_tasks[i] && all_tasks[i]->pid == pid)
            return all_tasks[i];*/
    //spin_lock(&all_tasks_lock);
    list_foreach_raw(&all_tasks, elem) {
        struct task *task = list_entry(elem, struct task, all_elem);

        if (task->pid == pid) {
            ret = task;
            break;
        }
    }
    //spin_unlock(&all_tasks_lock); 

    return ret;
}

void
task_join_pg(struct task *task, pid_t pgid)
{
    task->pgid = pgid;
}

struct task *
create_user_task(void (*func)(void))
{
    pagedir_t mm = new_page_directory();
    return create_user_task_withmm(1, mm, func);
}

struct task *
create_user_task_fork(void (*func)(void))
{
    struct task *new;
    pagedir_t mm = copy_page_dir(current_task->mm);

    /* mark the pages COW */
    mark_all_user_pages_cow(current_task->mm);
    mark_all_user_pages_cow(mm);

    /* XXX: fixme: undo COW */
    new = create_user_task_withmm(0, mm, func);
    if (!new)
        return NULL;

    //printk("fork: %d was created by pid %d(%s)\n", new->pid, current_task->pid,
            //current_task->comm);

    /* copy the filetable */
    copy_filetable(new, current_task);

    /* copy VMAs */
    copy_vmas(new, current_task);

    /* copy signals */
    copy_signals(new, current_task);

    /* copy CWD */
    free(new->cwd);
    new->cwd = strdup(current_task->cwd);

    /* copy SSE data */
    memcpy(new->sse_save, current_task->sse_save, 512);

    /* copy controlling terminal */
    new->ctty = current_task->ctty;

    /* copy BRK stuff */
    new->bstate.logical_brk = current_task->bstate.logical_brk;
    new->bstate.actual_brk = current_task->bstate.actual_brk;
    new->bstate.entry = current_task->bstate.entry;
    new->bstate.base_brk = current_task->bstate.base_brk;

    /* copy the IRQ stack so registers get refilled correctly */
    memcpy(new->irq_stack_bot, current_task->irq_stack_bot, 0x1000);

    new->comm = strdup(current_task->comm);

    /*printk("current_task->regs at 0x%x bot 0x%x top 0x%x(%s) off 0x%x\n",
            current_task->sys_regs,
            current_task->irq_stack_bot,
            current_task->irq_stack_top,
            current_task->sys_regs > current_task->irq_stack_bot &&
                (void *)current_task->sys_regs + sizeof(struct pt_regs) <= current_task->irq_stack_top
                    ? "ON IRQ STACK" : "somewhere else",
            (int)current_task->sys_regs - (int)current_task->irq_stack_bot);*/

    //dump_registers(current_task->sys_regs);

    /* set the registers */
    new->regs = (void *) ((int)new->irq_stack_bot + ((int)current_task->sys_regs - (int)current_task->irq_stack_bot));

    //memcpy((void *) 0xD0000000 - 0x1000, (void *)VIRT_BASE - 0x1000, 0x1000);
    //new->regs->esp += sizeof(struct pt_regs);

    //printk("new->regs at 0x%x bot 0x%x off 0x%x\n",
            //new->regs,
            //new->irq_stack_bot,
            //(int)new->regs - (int)new->irq_stack_bot);

    new->regs->eax = 0;
    new->sys_regs = new->regs;
    //dump_registers(new->regs);

    /* prefault the stack */
    //__do_cow(new, new->regs->esp);

    //printk("fork is done\n");

    //new->sys_regs = new->regs;

    return new;
}

void
kernel_task_exit()
{
    current_task->state = TASK_ZOMBIE;
    sched_yield();
}

void
sched_add_rq(struct task *task)
{
    //printk("%s: task->pid: %d task->regs: 0x%x\n", __func__, task->pid, task->regs);
    /*for (int i = 0; i < 128; i ++) {
        if (all_tasks[i] == 0) {
            all_tasks[i] = task;
            return;
        }
    }*/
    //spin_lock(&all_tasks_lock);
    list_push_back(&all_tasks, &task->all_elem);
    //spin_unlock(&all_tasks_lock); 
    return;
    panic("RQ is full\n");
}

extern void init_task(void);
void __noreturn
__idle_thread(void)
{
    struct task *n;
    __preempt_enabled = 1;
    arch_switch_timer_sched();

    /* XXX 4/22/17 review: I am not entirely sure why this is/was
     *                     useful, so I removed it
     */
    /* simulate a timer interrupt to get some values to regs */
    //asm volatile("mov $0xC0FFEEEE, %%eax; int $32":::"eax");
    //panic_on(current_task->regs->eax != 0xC0FFEEEE, "pt_regs is not stable");

    /* start another task */

    n = create_kernel_task(init_task);
    panic_on(n == NULL, "failed to create init task\n");
    sched_add_rq(n);

    current_task->comm = "swapper";

    late_init();

    __not_reached();
}

struct task *
pick_next_task(void)
{
    //printk("%s\n", __func__);
    extern uint32_t __pit_ticks;
    struct list_elem *elem;
    struct task *task = current_task, *t;

    if (__task_need_cleanup > 0) {
        list_foreach_raw(&all_tasks, elem) {
            t = list_entry(elem, struct task, all_elem);

            if (t->state == TASK_DYING)
                task_exit(t);
        }
        __task_need_cleanup = 0;
    }

    //spin_lock(&all_tasks_lock);
/*retry:
    for (int i = last_task; i < 128; i ++) {
        if (all_tasks[i] != 0) { 
            if (all_tasks[i]->state == TASK_DYING) {
                task_exit(all_tasks[i]);
                continue;
            } else if (all_tasks[i]->state == TASK_SLEEPING &&
                    all_tasks[i]->wake_time <= __pit_ticks) {
                all_tasks[i]->state = TASK_PREEMPTED;
            } else if (!task_runnable(all_tasks[i]))
                continue;
            last_task = i + 1;
            //if (all_tasks[i]->pid == 2)
                //printk("pid:%d\n", all_tasks[i]->pid);
            return all_tasks[i];
        }
    }

    if (last_task == 0)
        panic("No task to run\n");
    last_task = 0;
    */

retry:
    elem = list_next(&task->all_elem);
    if (elem == list_tail(&all_tasks))
        elem = list_begin(&all_tasks);
    task = list_entry(elem, struct task, all_elem);

    if (task->state == TASK_DYING) {
        //printk("rejected %d because it's dying\n", task->pid);
        task_exit(task);
        goto retry;
    } else if (task->state == TASK_SLEEPING && task->wake_time <= __pit_ticks) {
        task->state = TASK_PREEMPTED;
    } else if (!task_runnable(task)) {
        //printk("rejected %d because it was not runnable\n", task->pid);
        goto retry;
    }

    //spin_unlock(&all_tasks_lock); 
    return task;
}

void
intr_yield(struct pt_regs *r)
{
    DISABLE_IRQ();
    current_task->regs = r;
    //current_task->regs->ss = 0x23;
    reschedule();
}

void
sched_yield()
{
    asm volatile("int $0x2F");
}

void
__reschedule_to(struct task *next)
{
    next->time_ran = 0;
    next->state = TASK_RUNNING;
    //current_task->sys_regs = current_task->regs;
    do_sse_save(current_task);
    current_task = next;

    if (current_task->mm)
        activate_pgd(current_task->mm);
    //else activate_pgd(kernel_pgd);
    
    tss_update(current_task);

    if (!(next->flags & TFLAG_NO_SIGNAL) && task_has_pending_signals(next)) {
        //task->sys_regs = task->regs;
        signal_handle(next);
    }

    if (next->state != TASK_RUNNING)
        return;

    next->flags &= ~TFLAG_NO_SIGNAL;

    do_sse_restore(next);
    /* switch stack */
    asm volatile("movl %0, %%esp;"
                 "movw $0x20, %%dx;"
                 "movb $0x20, %%al;"
                 "outb %%al, %%dx;"
                 "sti;"
                 "jmp intr_exit"::"r"(next->regs));
    __not_reached();
}

void
reschedule_to(struct task *next)
{
retry:
    __reschedule_to(next);
    next = pick_next_task();
    goto retry;
}

void
reschedule(void)
{
    struct task *next;
    DISABLE_IRQ();
    if (__preempt_enabled == 0)
        reschedule_to(current_task);

    if (current_task->state == TASK_RUNNING)
        current_task->state = TASK_PREEMPTED;
    next = pick_next_task();
    reschedule_to(next);
}

void
sched_tick(struct pt_regs *r)
{
    current_task->time_ran ++;
    current_task->regs = r;
    //printk("TICK\n");
    if (current_task->time_ran > TIME_SLICE)
        reschedule();
}
