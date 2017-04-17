#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/fs.h>
#include <levos/elf.h>
#include <levos/task.h>
#include <levos/arch.h>
#include <levos/errno.h>
#include <levos/uname.h>
#include <levos/socket.h>

static void
syscall_undefined(uint32_t no)
{
    printk("WARNING: undefined systemcall %d\n", no);
}

static int
__verify_buffer(uint32_t start, size_t sz)
{
    int i;

    //printk("%s: start 0x%x sz %d\n", __func__, start, sz);

    /* is the buffer a valid user address? */
    if (start > VIRT_BASE || start + sz > VIRT_BASE) {
        //printk("Case KERN\n");
        return 1;
    }

    /* if a null pointer or looks like one then bail */
    if (start < 4096) {
        //printk("Case NULL\n");
        return 1;
    }

    if (start % 0x1000 == 0) {
        //printk("Case 1\n");
        /* we are on a page boundary, should be easy to check */
        for (i = 0; i < sz; i += 0x1000) {
            if (!page_mapped_curr(start + i))
                return 1;
        }
    } else {
        /* first, check if the size concerns the next page */
        uint32_t this_page = PG_RND_DOWN(start);
        uint32_t next_page = PG_RND_DOWN(start + sz);
        //printk("Case 2 this: 0x%x next: 0x%x\n", this_page, next_page);

        /* the buffer is as a whole on the same page */
        if (this_page == next_page) {
            //printk("Case 2.1\n");
            if (page_mapped_curr(start)) {
                //printk("Case 2.1.T\n");
                return 0;
            }
            //printk("Case 2.1.F\n");
            return 1;
        } else {
            //printk("Case 2.2");
            /* check the next pages */
            for (i = 0; i < sz; i += 0x1000) {
                if (!page_mapped_curr(start + i)) {
             //       printk("Case 2.2.F");
                    return 1;
                }
            }
            //printk("Case 2.2.T");
            return 0;
        }
    }

    /* all done, the buffer is valid */
    return 0;
}

static int
verify_buffer(void *p, size_t sz)
{
    return __verify_buffer((uint32_t) p, sz);
}

int
verify_buffer_string(char *string, int max)
{
    int i;
    char c;
    char *p = (char *)string;

    /* First check if the whole buffer would fit anyway */
    if (verify_buffer(string, max) == 0)
        return 0;

    /* check if the first byte is valid */
    if (verify_buffer(string, 0) != 0)
        return 1;

    /*
     * the whole buffer doesnt fit, check byte-by-byte until a zero
     * byte is found
     */
    for (i = 0; i < max; i ++) {
        c = p[i];

        /* found end, good to go */
        if (c == 0)
            return 0;

        /* verify that the next access is correct */
        if (verify_buffer(&p[i+1], 0) != 0)
            return 1;
    }

    /* overflowed the max */
    return 1;
}


static int
sys_open(char *filename, int flags, int mode)
{
    struct file *f;
    struct task *task = current_task;
    int i;

    if (verify_buffer_string(filename, PATH_MAX))
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
    if (verify_buffer_string(fn, PATH_MAX))
        return -EFAULT;

    if (verify_buffer(st, sizeof(*st)))
        return -EFAULT;

    return vfs_stat(fn, st);
}

static int
sys_fstat(int fd, struct stat *st)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    if (verify_buffer(st, sizeof(*st)))
        return -EFAULT;

    f = current_task->file_table[fd];
    if (!f)
            return -EBADF;

    f->fops->fstat(f, st);
}

static int
sys_execve(char *fn, char **argvp, char **envp)
{
    int rc = 0;

    if (verify_buffer_string(fn, PATH_MAX))
        return -EFAULT;

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
    current_task->owner->exit_code = err_code;
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

    if (verify_buffer(buf, count))
        return -EFAULT;

    return f->fops->write(f, buf, count);
}

static int
sys_fork(void)
{
    struct task *child;
    struct task *parent = current_task;
    uint32_t addr = VIRT_BASE - 0x1000;//PG_RND_DOWN((int) parent->regs->esp);

    child = create_user_task_fork(NULL);

    /* copy stack */
    memcpy((void *) 0xD0000000 - 0x1000, (void *)VIRT_BASE - 0x1000, 0x1000);

    /* copy irq stack */
    memcpy(child->irq_stack_bot, parent->irq_stack_bot, 0x1000);

    /* set the registers to point on their stack */
    child->regs = (void *) ((unsigned long)parent->sys_regs - (unsigned long)parent->irq_stack_bot) + (unsigned long)child->irq_stack_bot;
    child->regs->eax = 0;

    sched_add_child(parent, child);
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

    if (verify_buffer(buf, count))
        return -EFAULT;

    return f->fops->read(f, buf, count);
}

int
sys_uname(struct uname *un)
{
    if (verify_buffer(un, sizeof(*un)))
        return -EFAULT;

    do_uname(un);

    return 0;
}

int
sys_socket(int domain, int family, int proto)
{
    int i;
    struct socket *sock;
    struct file *filp;

    sock = socket_new(domain, family, proto);
    if (((int) sock) < 0 && ((int)sock) > -4096)
        return (int) sock;

    filp = file_from_socket(sock);
    if (!filp) {
        socket_destroy(sock);
        return -ENOMEM;
    }

    for (i = 0; i < FD_MAX; i ++) {
        if (current_task->file_table[i] == NULL) {
            current_task->file_table[i] = filp;
            return i;
        }
    }

    return -EMFILE;
}

int
sys_connect(int sockfd, void *sockaddr, size_t len)
{
    struct file *f;
    struct socket *sock;

    if (sockfd < 0 || sockfd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[sockfd];
    if (!f)
        return -EBADF;

    if (f->type != FILE_TYPE_SOCKET)
        return -ENOTSOCK;

    if (verify_buffer(sockaddr, len))
        return -EFAULT;

    sock = f->priv;

    return sock->sock_ops->connect(sock, sockaddr, len);
}

int
sys_waitpid(pid_t pid, int *wstatus, int opts)
{
    struct process *target;

    if (verify_buffer(wstatus, sizeof(int)))
        return -EFAULT;

    if (pid == -1) {
        /* wait for any child */
    } else if (pid < -1) {
        /* wait for any child of ABS(pid) */
    } else if (pid == 0) {
        /* wait for any child in the current proc group */
    } else {
        //printk("CASE 1\n");
        /* wait for specific child */
        target = sched_get_child(current_task, pid);
        if (target == NULL)
            return -ECHILD;

        //printk("CASE 1a\n");

        if (target->task == NULL) {
            //printk("CASE 1b\n");
            /* the task has already finished, return */
            *wstatus = 0xDEADC0DE; /* TODO */
            return 0;
        }

        *wstatus = task_do_wait(current_task, target);

        return 0;
    }

    return -ENOSYS;
}

/*
int
sys_getdents(int fd, struct dirent *buf, int count)
{
    return -ENOSYS;
}
*/

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
        case 0x7:
            return sys_waitpid((pid_t) a, (int *) b, (int) c);
        case 0xB:
            return sys_execve((char *) a, (char **) b, (char **) c);
        case 0x11:
            return sys_socket((int) a, (int) b, (int) c);
        case 0x12:
            return sys_stat((char *) a, (struct stat *) b);
        case 0x14:
            return sys_getpid();
        case 0x1c:
            return sys_fstat((int) a, (struct stat *) b);
        case 0x1f:
            return sys_connect((int) a, (void *) b, (size_t) c);
        case 0x6d:
            return sys_uname((struct uname *) a);
        //case 0x8d:
            //return sys_getdents((int) a, (struct dirent *) b, (int) c);
    }

    syscall_undefined(no);
    return -ENOSYS;
}
