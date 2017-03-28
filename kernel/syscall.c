#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/fs.h>
#include <levos/elf.h>
#include <levos/task.h>
#include <levos/arch.h>
#include <levos/errno.h>
#include <levos/uname.h>

static void
syscall_undefined(uint32_t no)
{
    printk("WARNING: undefined systemcall %d\n", no);
}


static int
sys_open(char *filename, int flags, int mode)
{
    struct file *f;
    struct task *task = current_task;
    int i;

    if (filename == NULL)
        return -EFAULT;

    f = vfs_open(filename);

    if (f == NULL || (int)f == -ENOENT)
        return -ENOENT;

    for (i = 0; i < FD_MAX; i ++) {
        if (task->file_table[i] == NULL) {
            task->file_table[i] = f;
            return i;
        }
    }
    
    return -ENFILE;
}

static int
sys_close(int fd)
{
    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    if (!current_task->file_table[fd])
        return -EBADF;

    current_task->file_table[fd] = NULL;
    return 0;
}

static int
sys_getpid(void)
{
    return current_task->pid;
}

static int
sys_stat(char *fn, struct stat *st)
{
    return vfs_stat(fn, st);
}

static int
sys_fstat(int fd, struct stat *st)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
            return -EBADF;

    if (!st)
        return -EFAULT;

    f->fops->fstat(f, st);
}

static int
sys_execve(char *fn, char **argvp, char **envp)
{
    int rc = 0;

    char *kfn = malloc(strlen(fn) + 1);
    if (!kfn)
        return -ENOMEM;
    memset(kfn, 0, strlen(fn) + 1);
    memcpy(kfn, fn, strlen(fn));

    struct file *f = vfs_open(kfn);
    if (f == NULL)
        return -ENOENT;

    __flush_tlb();

    rc = load_elf(f);
    if (rc)
        return rc;

    exec_elf();
    return rc;
}

static int
sys_exit(int err_code)
{
    current_task->state = TASK_DYING;
    current_task->exit_code = err_code;
    //printk("pid %d exited with exit code %d\n", current_task->pid, err_code);
    sched_yield();
    __not_reached();
}

static int
sys_write(int fd, char *buf, size_t count)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    if (f->isdir)
        return -EISDIR;

    if (count == 0)
        return 0;

    if (buf == NULL)
        return -EFAULT;

    return f->fops->write(f, buf, count);
}

void
do_fork()
{
    while(1);
}

static int
sys_fork(void)
{
    struct task *child;
    struct task *parent = current_task;
    uint32_t addr = VIRT_BASE - 0x1000;//PG_RND_DOWN((int) parent->regs->esp);
    char *buf = malloc(0x1000);
    memcpy(buf, addr, 0x1000);
    uint32_t save = parent->sys_regs;

    child = create_user_task_fork(do_fork);

    /* copy stack */
    memcpy((unsigned int) 0xD0000000 - 0x1000, (unsigned int)VIRT_BASE - 0x1000, 0x1000);

    /* copy irq stack */
    memcpy(child->irq_stack_bot, parent->irq_stack_bot, 0x1000);
    child->regs = ((unsigned long)parent->sys_regs - (unsigned long)parent->irq_stack_bot) + (unsigned long)child->irq_stack_bot;
    child->regs->eax = 0;

    sched_add_rq(child);
    return child->pid;
}

static int
sys_read(int fd, char *buf, size_t count)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    if (f->isdir)
        return -EISDIR;

    if (count == 0)
        return 0;

    if (buf == NULL)
        return -EFAULT;

    return f->fops->read(f, buf, count);
}

int
sys_uname(struct uname *un)
{
    if (un == NULL)
        return -EFAULT;

    do_uname(un);

    return 0;
}

int
sys_getdents(int fd, struct dirent *buf, int count)
{
    return -ENOSYS;
}

int
syscall_hub(int no, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    //printk("SYSCALL %d\n", no);
    switch(no) {
        case 0x1:
            sys_exit((int) a);
            __not_reached();
        case 0x2:
            return sys_fork();
        case 0x3:
            return sys_read((int) a, (char *)b, (size_t) c);
        case 0x4:
            return sys_write((int) a, (char *)b, (size_t) c);
        case 0x5:
            return sys_open((char *) a, (int) b, (int) c);
        case 0x6:
            return sys_close((int) a);
        case 0xB:
            return sys_execve((char *) a, (char **) b, (char **) c);
        case 0x12:
            return sys_stat((char *) a, (struct stat *) b);
        case 0x14:
            return sys_getpid();
        case 0x1c:
            return sys_fstat((int) a, (struct stat *) b);
        case 0x6d:
            return sys_uname((struct uname *) a);
        case 0x8d:
            return sys_getdents((int) a, (struct dirent *) b, (int) c);
    }

    syscall_undefined(no);
    return -ENOSYS;
}
