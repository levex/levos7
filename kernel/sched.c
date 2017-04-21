#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/task.h>
#include <levos/x86.h> /* FIXME */
#include <levos/intr.h>
#include <levos/arch.h>
#include <levos/palloc.h>
#include <levos/page.h>
#include <levos/list.h>

#define TIME_SLICE 5

static pid_t last_pid = 0;
struct task *current_task;

static struct list all_tasks;

struct list *__ALL_TASKS_PTR = &all_tasks;

//static int last_task;

static struct list zombie_processes;

void __noreturn late_init(void);
void __noreturn __idle_thread(void);

void intr_yield(struct pt_regs *);
void sched_yield(void);
void reschedule(void);

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

    //memset(all_tasks, 0, sizeof(struct task *) * 128);
    //all_tasks[0] = current_task;
    list_push_back(&all_tasks, &current_task->all_elem);
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
    task->pid = allocate_pid();
    task->ppid = 0;
    task->pgid = 0;
    task->state = TASK_PREEMPTED;
    task->time_ran = 0;
    task->parent = NULL;
    task->exit_code = 0;
    task->mm = kernel_pgd;
    task->flags = 0;
    list_init(&task->children_list);
    signal_init(task);
    task->owner = process_create(task);
    if (!task->owner) {
        //free_pid(task->pid); // TODO
        free(task);
        return -ENOMEM;
    }

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
task_block(struct task *task)
{
    task->state = TASK_BLOCKED;
    //printk("%s: pid %d\n", __func__, task->pid);
    if (task == current_task)
        sched_yield();
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
    //printk("%s: pid %d\n", __func__, task->pid);
    task->state = TASK_PREEMPTED;
}

void
setup_filetable(struct task *task)
{
    /* for stdin, we currently just pass through to the serial,
     * but in the future we really should implement ttys and
     * give the init process a different stdin
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
close_filetable(struct task *task)
{
    int i;

    for (i = 0; i < FD_MAX; i ++) {
        struct file *filp = task->file_table[i];

        if (filp && filp->fops->close)
            filp->fops->close(filp);
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

    //printk("TASK EXIT for %d\n", t->pid);

    /* check if there are waiters */
    if (!list_empty(&t->owner->waiters)) {
        struct list_elem *elem;
        /* wake them up */
        list_foreach_raw(&t->owner->waiters, elem) {
            struct task *waiter = list_entry(elem, struct task, wait_elem);

            task_unblock(waiter);
       }
    }

    if (t->flags & TFLAG_WAITED)
        process_destroy(t->owner);
    else {
        t->owner->task = NULL;
        list_push_back(&zombie_processes, &t->owner->elem);
    }

    close_filetable(t);
    free(t->irq_stack_bot);
    list_remove(&t->all_elem);
    free(t);
}

struct task *create_user_task_withmm(pagedir_t mm, void (*func)(void))
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

    /* get a stack for the task, since this is a kernel thread
     * we can use malloc
     */
    p_new_stack = palloc_get_page();
    if (!p_new_stack) {
        free(task);
        return NULL;
    }

    task->irq_stack_top = (uint32_t *) (((uint8_t *) malloc(0x1000)) + 0x1000);
    task->irq_stack_bot = (void *) ((uint32_t)task->irq_stack_top) - 0x1000;

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

int
task_do_wait(struct task *waiter, struct process *waited)
{
    panic_ifnot(waiter == current_task);

    process_add_waiter(waited, waiter);

    task_block(waiter);

    /* we've been woken up! */

    waiter->flags |= TFLAG_WAITED;

    /* figure out what happened */
    if (waited->task == NULL) {
        /* the task has exited */
        return WAIT_EXIT((uint8_t) waited->exit_code);
    } else
        panic("Unsupported wait!\n");

    return 0xffffffff;
}

struct task *
get_task_for_pid(pid_t pid)
{
    int i;
    struct list_elem *elem;

    /*for (i = 0; i < 128; i ++)
        if (all_tasks[i] && all_tasks[i]->pid == pid)
            return all_tasks[i];*/
    list_foreach_raw(&all_tasks, elem) {
        struct task *task = list_entry(elem, struct task, all_elem);

        if (task->pid == pid)
            return task;
    }

    return NULL;
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
    return create_user_task_withmm(mm, func);
}

struct task *
create_user_task_fork(void (*func)(void))
{
    pagedir_t mm = copy_page_dir(current_task->mm);
    mark_all_user_pages_cow(current_task->mm);
    mark_all_user_pages_cow(mm);
    return create_user_task_withmm(mm, func);
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
    list_push_back(&all_tasks, &task->all_elem);
    return;
    panic("RQ is full\n");
}

extern void init_task(void);
void __noreturn
__idle_thread(void)
{
    struct task *n;
    arch_switch_timer_sched();

    /* simulate a timer interrupt to get some values to regs */
    asm volatile("mov $0xC0FFEEEE, %%eax; int $32":::"eax");
    panic_on(current_task->regs->eax != 0xC0FFEEEE, "pt_regs is not stable");

    /* start another task */

    n = create_kernel_task(init_task);
    panic_on(n == NULL, "failed to create init task\n");
    sched_add_rq(n);

    late_init();

    __not_reached();
}

struct task *
pick_next_task(void)
{
    //printk("%s\n", __func__);
    extern uint32_t __pit_ticks;
    struct list_elem *elem;
    struct task *task = current_task;
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
        task_exit(task);
        goto retry;
    } else if (task->state == TASK_SLEEPING && task->wake_time <= __pit_ticks) {
        task->state = TASK_PREEMPTED;
    } else if (!task_runnable(task))
        goto retry;

    return task;
}

void
intr_yield(struct pt_regs *r)
{
    DISABLE_IRQ();
    current_task->regs = r;
    reschedule();
}

void
sched_yield()
{
    asm volatile("int $0x2F");
}

void
reschedule_to(struct task *next)
{
    next->time_ran = 0;
    next->state = TASK_RUNNING;
    current_task = next;
    //current_task->sys_regs = current_task->regs;

    if (current_task->mm)
        activate_pgd(current_task->mm);
    //else activate_pgd(kernel_pgd);
    
    tss_update(current_task);

    if (task_has_pending_signals(next))
        signal_handle(next);

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
reschedule(void)
{
    if (current_task->state == TASK_RUNNING)
        current_task->state = TASK_PREEMPTED;
    struct task *next = pick_next_task();
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
