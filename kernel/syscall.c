#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/fs.h>
#include <levos/elf.h>
#include <levos/task.h>
#include <levos/arch.h>
#include <levos/errno.h>
#include <levos/uname.h>
#include <levos/signal.h>
#include <levos/socket.h>

#define ARGS_MAX 16
#define ENVS_MAX 16

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

char **
copy_array_from_user(char **ptr, int max)
{
    int c = 0, i = 0;
    int len = 0, sofar = 0;
    char *buf = NULL;
    char **arr = NULL;

    /* FIXME: proper check */

    for (c = 0; ptr[c] != NULL; c ++)
        len += strlen(ptr[c]) + 2;

    //printk("%s total len %d\n", __func__, len);

    buf = malloc(len);
    if (!buf)
        return ERR_PTR(-ENOMEM);

    arr = malloc((c + 1) * sizeof(uintptr_t));
    if (!arr) {
        free(buf);
        return ERR_PTR(-ENOMEM);
    }
    memset(arr, 0, (c + 1) * sizeof(uintptr_t));

    for (i = 0; i < c; i ++) {
        memcpy(buf + sofar, ptr[i], strlen(ptr[i]) + 1);
        //printk("buf+sofar(%d) is %s\n", sofar, buf + sofar);
        arr[i] = buf + sofar;
        sofar += strlen(ptr[i]) + 1;
    }

    return arr;
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
    if (IS_ERR(f)) {
        int err = PTR_ERR(f);
        printk("err is %s\n", errno_to_string(err));
        if (err == -ENOENT) {
            if (flags & O_CREAT) {
                f = vfs_create(filename);
                if (IS_ERR(f)) {
                    printk("FATAL\n");
                    return PTR_ERR(f);
                }

                goto xc;
            }

            return -ENOENT;
        }

        return err;
    }

    if (f->isdir && (flags & O_WRONLY || flags & O_RDWR))
        return -EISDIR;
    
    if (flags & O_EXCL && flags & O_CREAT) {
        vfs_close(f);
        return -EEXIST;
    }

xc:
    for (i = 0; i < FD_MAX; i ++) {
        if (task->file_table[i] == NULL) {
            task->file_table[i] = f;
            f->refc ++;
            return i;
        }
    }
    
    return -EMFILE;
}

static int
do_close(int fd)
{
    struct file *f = current_task->file_table[fd];
    f->refc --;

    current_task->file_table[fd] = NULL;
    if (f->refc == 0) {
        if (f->fops->close)
            f->fops->close(f);
    }

    return 0;
}

static int
sys_close(int fd)
{
    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    if (!current_task->file_table[fd])
        return -EBADF;

    return do_close(fd);
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

    argvp = copy_array_from_user(argvp, ARGS_MAX);
    if (IS_ERR(argvp) && PTR_ERR(argvp) == -EFAULT)
        return -EFAULT;

    envp = copy_array_from_user(envp, ENVS_MAX);
    if (IS_ERR(envp) && PTR_ERR(envp) == -EFAULT)
        return -EFAULT;

    rc = load_elf(f, argvp, envp);
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

    /* TODO: support the opts field */
    if (opts != 0)
        return -EINVAL;

    if (pid == -1) {
        /* wait for any child */
    } else if (pid < -1) {
        /* wait for any child of ABS(pid) */
    } else if (pid == 0) {
        /* wait for any child in the current proc group */
    } else {
        /* wait for specific child */
        target = sched_get_child(current_task, pid);
        if (target == NULL)
            return -ECHILD;

        if (target->task == NULL) {
            /* the task has already finished, return */
            *wstatus = WAIT_EXIT(target->exit_code);
            return 0;
        }

        *wstatus = task_do_wait(current_task, target);

        return 0;
    }

    return -ENOSYS;
}

int
sys_lseek(int fd, size_t off, int whence)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    if (whence == SEEK_SET)
        f->fpos = off;
    else if (whence == SEEK_CUR)
        f->fpos += off;
    else if (whence == SEEK_END)
        f->fpos = 0; /* FIXME */

    /* FIXME: pipe support missing */

    return -EINVAL;
}

int
sys_dup(int fd)
{
    struct file *f;
    int i;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    for (i = 0; i < FD_MAX; i ++) {
        if (current_task->file_table[i] == NULL) {
            current_task->file_table[i] = f;
            f->refc ++;
            return i;
        }
    }
}

int
sys_dup2(int fd, int to)
{
    struct file *f, *f2;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    if (to < 0 || to >= FD_MAX)
        return -EBADF;

    if (fd == to)
        return to;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    f2 = current_task->file_table[to];
    if (f2)
        do_close(to);

    current_task->file_table[to] = f;
    return to;
}

int
sys_readdir(int fd, struct linux_dirent *buf, int count)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    if (!f->isdir)
        return -ENOTDIR;

    if (verify_buffer(buf, sizeof(struct linux_dirent)))
        return -EFAULT;

    return f->fops->readdir(f, buf);
}

int
sys_signal(int signum, sighandler_t handler)
{
    if (!signal_is_signum_valid(signum))
        return -EINVAL;

    signal_register_handler(current_task, signum, handler);

    return 0;
}

int
sys_kill(int pid, int sig)
{
    if (sig == 0) {
        struct task *task = get_task_for_pid(pid);
        if (task)
            return 0;
        return -ESRCH;
    }

    if (sig < MIN_SIG || sig > MAX_SIG)
        return -EINVAL;

    if (pid == 0) {
        send_signal_group_of(current_task, sig);
        return 0;
    }

    if (pid > 0) {
        struct task *task = get_task_for_pid(pid);
        send_signal(task, sig);
        return 0;
    }

    if (pid == -1) {
        /* XXX: send to every process we have permission,
         * we for the moment denote this as the session
         */
        send_signal_session_of(current_task, sig);
        return 0;
    }

    if (pid < -1) {
        int pgid = -pid;
        send_signal_group(pgid, sig);
        return 0;
    }
    
    __not_reached();
}

int
sys_setsid(void)
{
    if (current_task->pg_leader)
        return -EPERM;

    current_task->sid = current_task->pid;
    current_task->pgid = current_task->pid;
    current_task->pg_leader = NULL;

    return current_task->pid;
}

int
sys_setpgid(pid_t pid, pid_t pgid)
{
    struct task *task, *pg_leader;

    if (pgid < 0)
        return -EINVAL;

    if (pid == 0)
        pid = current_task->pid;

    if (pgid == 0)
        pgid = pid;

    pg_leader = get_task_for_pid(pgid);
    /* err, this is not mentioned by POSIX, but this seems
     * like the thing to do?
     */
    if (!pg_leader)
        return -ESRCH;

    /* if we are moving a task, then it must match the session ids */
    if (task->pgid != pgid &&
            task->sid != pg_leader->sid)
        return -EPERM;

    task->pgid = pgid;
    return 0;
}

int
sys_getpgid(pid_t pid)
{
    struct task *task;
    if (pid == 0)
        return current_task->pgid;

    task = get_task_for_pid(pid);
    if (task)
        return task->pgid;

    return -ESRCH;
}


int
syscall_hub(int no, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    //printk("SYSCALL pid %d no %d\n", current_task->pid, no);
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
        case 0x13:
            return sys_lseek((int) a, (size_t) b, (int) c);
        case 0x14:
            return sys_getpid();
        case 0x1c:
            return sys_fstat((int) a, (struct stat *) b);
        case 0x1f:
            return sys_connect((int) a, (void *) b, (size_t) c);
        case 0x25:
            return sys_kill((int) a, (int) b);
        case 0x29:
            return sys_dup((int) a);
        case 0x30:
            return sys_signal((int) a, (sighandler_t) b);
        case 0x39:
            return sys_setpgid((int) a, (int) b);
        case 0x3f:
            return sys_dup2((int) a, (int) b);
        case 0x42:
            return sys_setsid();
        case 0x59:
            return sys_readdir((int) a, (struct linux_dirent *) b, (int) c);
        case 0x6d:
            return sys_uname((struct uname *) a);
        case 0x84:
            return sys_getpgid((int) a);
    }

    syscall_undefined(no);
    return -ENOSYS;
}
