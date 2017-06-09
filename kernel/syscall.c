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
#include <levos/work.h>

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
    if (p > VIRT_BASE || p + sz > VIRT_BASE)
        return 1;
    vma_try_prefault(current_task, p, sz);
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

void
free_copied_array(char **arr)
{
    free(arr[0]);
    free(arr);
}


static int
sys_open(char *__filename, int flags, int mode)
{
    struct file *f;
    struct task *task = current_task;
    char *filename, *full_filename;
    int i, need_free = 0;

    //printk("%s\n", __func__);

    if (verify_buffer_string(__filename, PATH_MAX))
        return -EFAULT;

    if (flags & ~(O_TRUNC | O_NOCTTY | O_CLOEXEC | O_CREAT | O_WRONLY | O_RDWR | O_EXCL))
        printk("pid %d: unsupported openflag detected in 0x%x isol: 0x%x\n",
                current_task->pid, flags,
                flags & ~(O_TRUNC | O_NOCTTY | O_CLOEXEC | O_CREAT | O_WRONLY | O_RDWR | O_EXCL));

    /* we don't support O_TRUNC and O_RDONLY */
    if (flags & O_TRUNC &&
            flags & O_RDWR == 0 &&
            flags & O_WRONLY == 0)
        return -EINVAL;

    /* can I derefence this? fixme */
    if (__filename[0] != '/') {
        filename = __canonicalize_path(current_task->cwd, __filename);
        need_free = 1;
    } else filename = __filename;

    full_filename = strdup(filename);

    //printk("full filename is \"%s\"\n", full_filename);

    f = vfs_open(full_filename);
    if (IS_ERR(f)) {
        int err = PTR_ERR(f);
        if (err == -ENOENT) {
            if (flags & O_CREAT) {
                f = vfs_create(full_filename);
                if (IS_ERR(f)) {
                    if (need_free) free(filename);
                    free(full_filename);
                    //printk("%s r0: %s\n", __func__, errno_to_string(PTR_ERR(f)));
                    return PTR_ERR(f);
                }

                goto xc;
            }

            //printk("%s doesn't exist and O_CREAT wasnt supplied\n",
                        //full_filename);
            if (need_free) free(filename);
            free(full_filename);
            //printk("%s r1: %s\n", __func__, errno_to_string(-ENOENT));
            return -ENOENT;
        }

        if (need_free) free(filename);
        free(full_filename);
        //printk("%s r2: %s\n", __func__, errno_to_string(err));
        return err;
    }

    if (flags & O_TRUNC) {
        int err = vfs_truncate(f);
        if (err) {
            if (need_free) free(filename);
            free(full_filename);
            vfs_close(f);
            return err;
        }
    }

    if (f->isdir && (flags & O_WRONLY || flags & O_RDWR)) {
        if (need_free) free(filename);
        free(full_filename);
        //printk("%s r3: %s\n", __func__, errno_to_string(-EISDIR));
        return -EISDIR;
    }
    
    if (flags & O_EXCL && flags & O_CREAT) {
        vfs_close(f);
        if (need_free) free(filename);
        free(full_filename);
        //printk("%s r4: %s\n", __func__, errno_to_string(-EEXIST));
        return -EEXIST;
    }

xc:
    f->mode = mode;
    for (i = 0; i < FD_MAX; i ++) {
        if (task->file_table[i] == NULL) {
            task->file_table[i] = f;
            //f->full_path = full_filename;
            free(full_filename); /* todo: figure this out */
            if (need_free) free(filename);
            //printk("%s r6: fd %d\n", __func__, i);
            return i;
        }
    }
    
    if (need_free) free(filename);
    //printk("%s r5: %s\n", __func__, errno_to_string(-EMFILE));
    return -EMFILE;
}

static int
do_close(int fd)
{
    struct file *f = current_task->file_table[fd];
    //printk("%s: fd %d refc %d\n", __func__, fd, f->refc);
    vfs_close(f);
    current_task->file_table[fd] = NULL;
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
sys_stat(char *__fn, struct stat *st)
{
    char *fn, need_free = 0;
    int rc;

    if (verify_buffer_string(__fn, PATH_MAX))
        return -EFAULT;

    if (verify_buffer(st, sizeof(*st)))
        return -EFAULT;

    if (__fn[0] != '/') {
        fn =  __canonicalize_path(current_task->cwd, __fn);
        need_free = 1;
    } else fn = __fn;

    memset(st, 0, sizeof(*st));

    //printk("%s: canonical filename: %s\n", __func__, fn);

    rc = vfs_stat(fn, st);
    if (need_free)
        free(fn);

    return rc;
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

    memset(st, 0, sizeof(*st));

    /*
    printk("well fops is at 0x%x\n", f->fops);
    if (f->full_path) {
        printk("FSTAT: %s\n", f->full_path);
    } else {
        printk("doesnt have a full_path so the fops is at 0x%x\n", f->fops);
    }*/

    return f->fops->fstat(f, st);
}

static int
sys_execve(char *fn, char **u_argvp, char **u_envp)
{
    int rc = 0;
    char **argvp, **envp;

    if (verify_buffer_string(fn, PATH_MAX))
        return -EFAULT;

    char *kfn = malloc(strlen(fn) + 1);
    if (!kfn)
        return -ENOMEM;
    memset(kfn, 0, strlen(fn) + 1);
    memcpy(kfn, fn, strlen(fn));

    char *full_path = __canonicalize_path(current_task->cwd, kfn);
    free(kfn);
    kfn = full_path;

    struct file *f = vfs_open(kfn);
    if (f == NULL || IS_ERR(f)) {
        free(kfn);
        return -ENOENT;
    }

    __flush_tlb();

    argvp = copy_array_from_user(u_argvp, ARGS_MAX);
    if (IS_ERR(argvp) && PTR_ERR(argvp) == -EFAULT) {
        free(kfn);
        return -EFAULT;
    }

    envp = copy_array_from_user(u_envp, ENVS_MAX);
    if (IS_ERR(envp) && PTR_ERR(envp) == -EFAULT) {
        free_copied_array(argvp);
        free(kfn);
        return -EFAULT;
    }

    //printk("EXECVE %d: FULL_PATH %s KFN %s\n", current_task->pid, f->full_path, kfn);

    rc = elf_probe(f);
    if (rc)
        return rc;

    vma_unload_all(current_task);

    map_unload_user_pages(current_task->mm);
    //current_task->mm = copy_page_dir(kernel_pgd);
    //__flush_tlb();

    vma_init(current_task);

    free(kfn);

    current_task->bstate.base_brk =
        current_task->bstate.logical_brk =
        current_task->bstate.actual_brk = 0;
    
    rc = load_elf(f, argvp, envp);
    if (rc) {
        free_copied_array(envp);
        free_copied_array(argvp);
        free(kfn);
        return rc;
    }

    __flush_tlb();

    task_do_cloexec(current_task);

    signal_reset_for_execve(current_task);

    /* the original reference can now be given up */
    vfs_close(f);

    //printk("executing elf...\n");

    exec_elf();

    printk("WAAAAAT\n");
    return rc;
}

static int
sys_exit(int err_code)
{
    if (task_has_pending_signals(current_task))
        current_task->flags |= TFLAG_NO_SIGNAL;

    current_task->state = TASK_DYING;
    current_task->exit_code = err_code;
    current_task->owner->exit_code = err_code;
    current_task->owner->status = TASK_EXITED;
    extern int __task_need_cleanup;
    __task_need_cleanup ++;
    //printk("pid %d(%s) exited with exit code %d\n", current_task->pid, current_task->comm, err_code);
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

    child = create_user_task_fork(NULL);

    /* copy stack */
    // memcpy((void *) 0xD0000000 - 0x1000, (void *)VIRT_BASE - 0x1000, 0x1000);

    /* copy irq stack */
    // memcpy(child->irq_stack_bot, parent->irq_stack_bot, 0x1000);

    /* set the registers to point on their stack */
    // child->regs = (void *) ((unsigned long)parent->sys_regs - (unsigned long)parent->irq_stack_bot) + (unsigned long)child->irq_stack_bot;
    //
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

    //vma_dump(current_task);

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
    //if (opts != 0)
        //return -EINVAL;
    
    //printk("%s: %d(%s) wants to wait on pid %d(%s)\n",
            //__func__, current_task->pid, current_task->comm,
            //pid, get_comm_for_pid(pid));

    if (pid == -1) {
        struct list_elem *elem;

        /* check zombie tasks */
        extern struct list zombie_processes;
retry:
        list_foreach_raw(&zombie_processes, elem) {
            struct process *proc = list_entry(elem, struct process, elem);

            if (proc->ppid == current_task->pid) {
                if (wstatus) {
                    if (proc->status == TASK_EXITED)
                        *wstatus = WAIT_EXIT(proc->exit_code);
                    else if (proc->status == TASK_DEATH_BY_SIGNAL)
                        *wstatus = WAIT_SIG(proc->exit_code);
                    else {
                        printk("CRITICAL: we have a zombie not killed by a signal,"
                                    " or exited\n");
                    }
                }
                list_remove(&proc->elem);
                process_destroy(proc);
                return proc->pid;
            }
        }

        /* check for status changes */
        list_foreach_raw(&current_task->children_list, elem) {
            struct task *task = list_entry(elem, struct task, children_elem);

            if (!(task->flags & TFLAG_WAITED) && task->owner->status == TASK_SUSPENDED) {
                task->flags |= TFLAG_WAITED;
                *wstatus = WAIT_STOPPED(task->owner->exit_code);
                return task->pid;
            }
        }

        /* nope */
        if (opts & WNOHANG)
            return 0;
        else {
            sleep(10);
            goto retry;
        }
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
            //printk("the task waited on has finished, its exit_code was %d\n",
                    //target->exit_code);
            if (target->status == TASK_EXITED)
                *wstatus = WAIT_EXIT(target->exit_code);
            else if (target->status == TASK_DEATH_BY_SIGNAL)
                *wstatus = WAIT_SIG(target->exit_code);
            else panic("unknown waitpid(2) death\n");
            return pid;
        }

        /* if WNOHANG is specified, and since a child exists, return 0 */
        if (opts & WNOHANG)
            return 0;

        *wstatus = task_do_wait(current_task, target);

        return pid;
    }

    printk("CRITICAL: unsupported waitpid(2)\n");
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

    if (f->type == FILE_TYPE_PIPE)
        return -ESPIPE;

    if ((whence == SEEK_SET && off < 0) ||
            (whence == SEEK_CUR && f->fpos + off < 0) ||
            (whence == SEEK_END && f->fpos + off < 0))
        return -EINVAL;

    if (whence == SEEK_SET)
        f->fpos = off;
    else if (whence == SEEK_CUR)
        f->fpos += off;
    else if (whence == SEEK_END)
        f->fpos = f->length + off;

    return f->fpos;
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
            vfs_inc_refc(f);
            return i;
        }
    }

    return -EMFILE;
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

    /* clear FD_CLOEXEC */
    current_task->file_table_flags[to] = 0;

    vfs_inc_refc(f);
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

sighandler_t
sys_signal(int signum, sighandler_t handler)
{
    sighandler_t old;

    if (!signal_is_signum_valid(signum))
        return -EINVAL;

    old = signal_get_disp(current_task, signum);

    signal_register_handler(current_task, signum, handler);

    //if (signum == SIGCHLD) {
        //printk("pid %d(%s) registered SIGCHLD handler to 0x%x\n",
                //current_task->pid, current_task->comm, handler);
    //}

    return old;
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
        printk("kill: from %d to pgid %d\n", current_task->pid, pgid);
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

    task = get_task_for_pid(pid);
    if (!task)
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
sys_ioctl(int fd, int cmd, int arg)
{
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    if (!f->fops->ioctl)
        return -ENOTTY;

    return f->fops->ioctl(f, (unsigned long) cmd, (unsigned long) arg);
}

int
sys_pipe(int *fildes)
{
    int i, spc = 0;
    int kfds[2] = { -1, -1 };
    int rc;

    if (verify_buffer(fildes, sizeof(int) * 2))
        return -EFAULT;

    /* check if there is enough space in the file_table */
    for (i = 0; i < FD_MAX; i ++)
        if (current_task->file_table[i] == NULL) {
            if (kfds[0] == -1) {
                kfds[0] = i;
            } else if (kfds[1] == -1) {
                kfds[1] = i;
                break;
            }
        }

    if (kfds[0] == -1 || kfds[1] == -1)
        return -EMFILE;

    rc = do_pipe(current_task, kfds);
    if (rc)
        return rc;

    fildes[0] = kfds[0];
    fildes[1] = kfds[1];

    return 0;
}

int
sys_chdir(char *fn)
{
    int len;
    char *kbuf;
    char *ptr;

    if (verify_buffer_string(fn, PATH_MAX))
        return -EFAULT;

    len = strlen(fn);
    kbuf = malloc(len + 1);
    if (!kbuf)
        return -ENOMEM;

    memset(kbuf, 0, len + 1);
    memcpy(kbuf, fn, len);

    ptr = canonicalize_path(current_task->cwd, kbuf);
    if (IS_ERR(ptr)) {
        free(kbuf);
        return PTR_ERR(ptr);
    }

    free(current_task->cwd);
    free(kbuf);

    current_task->cwd = ptr;
    return 0;
}

int
sys_sbrk(int incr)
{
    struct task *t = current_task;
    struct bin_state *bs = &t->bstate;
    uintptr_t ret;

    //printk("%s: incr: %d\n", __func__, incr);

    if (incr == 0)
        return bs->logical_brk;

    if (incr == 0x37214000)
        panic("are you retarded\n");

    if (incr < 0) {
        ret = bs->logical_brk;

        //panic("WARNING: application %d is trying to free %d bytes of memory\n",
                //t->pid, -incr);

        bs->logical_brk -= (-1) * incr;

        return ret;
    }

    //while (incr % 0x1000)
        //incr ++;

    if (bs->logical_brk + incr < bs->actual_brk) {
        ret = bs->logical_brk;
        bs->logical_brk += incr;
        memset(ret, 0, incr);
        //printk("STATE EASY: actual: 0x%x logical 0x%x ret 0x%x\n",
               //bs->actual_brk, bs->logical_brk, ret);
        return ret;
    } else {
        int i, num = 1;
        uintptr_t ret = bs->logical_brk;
        uintptr_t i_ret = ret;
        ret = (ret + 0xfff) & ~0xfff; /* Rounds ret to 0x1000 in O(1) */
        bs->logical_brk += (ret - i_ret) + incr;

        while (bs->logical_brk > bs->actual_brk) {
            uintptr_t phys = palloc_get_page();
            map_page_curr(phys, bs->actual_brk, 1);
            //memset(bs->actual_brk, 0, 4096);
            bs->actual_brk += 0x1000;
        }

        memset(ret, 0, incr);

        //printk("STATE HARD: actual: 0x%x logical 0x%x ret 0x%x\n",
                //bs->actual_brk, bs->logical_brk, ret);
        return ret;
    }

    return -ENOMEM;
}

int
sys_getcwd(char *buf, unsigned long size)
{
    if (!size || !buf)
        return -EINVAL;

    if (verify_buffer(buf, size))
        return -EFAULT;

    if ((strlen(current_task->cwd) + 1) > size)
        return -ERANGE;

    memset(buf, 0, size);

    memcpy(buf, current_task->cwd, strlen(current_task->cwd));

    return 0;
}

int
sys_fcntl(int fd, unsigned long cmd, unsigned long arg)
{
    int i;
    struct file *f;

    if (fd < 0 || fd >= FD_MAX)
        return -EBADF;

    f = current_task->file_table[fd];
    if (!f)
        return -EBADF;

    switch (cmd) {
        case F_DUPFD:
            if (arg < 0 || arg >= FD_MAX)
                return -EINVAL;
            for (i = arg; i < FD_MAX; i ++) {
                if (current_task->file_table[i] == NULL) {
                    current_task->file_table[i] = current_task->file_table[fd];
                    vfs_inc_refc(current_task->file_table[i]);
                    return i;
                }
            }
            return -EMFILE;
        case F_GETFD:
            return current_task->file_table_flags[fd];
        case F_SETFD:
            current_task->file_table_flags[fd] |= arg;
            return 0;
    }

    printk("WARNING: Unimplement fnctl(2) cmd: %d with arg 0x%x\n", cmd, arg);
    return -ENOSYS;
}

int
sys_sysconf(int req)
{
    if (req == 4)
        return FD_MAX;

    if (req == 2)
        return 100;

    if (req == 8)
        return 4096;

    if (req == 11)
        return 32768;

    printk("unknown sysconf: %d\n", req);

    return -EINVAL;
}

int
sys_mkdir(char *pathname, int mode)
{
    char *fn, need_free = 0;
    int rc;

    if (verify_buffer_string(pathname, PATH_MAX))
        return -EFAULT;

    if (pathname[0] != '/') {
        fn =  __canonicalize_path(current_task->cwd, pathname);
        need_free = 1;
    } else fn = pathname;

    rc = vfs_mkdir(fn, mode);
    if (need_free)
        free(fn);

    return rc;
}

void
__deliver_alarm(void *aux)
{
    struct task *task = aux;

    send_signal(task, SIGALRM);
}

int
sys_alarm(int secs)
{
    int rc = 0;

    if (current_task->alarm_work) {
        int time = current_task->alarm_work->work_at - work_get_ticks();
        time /= 150;

        work_cancel(current_task->alarm_work);

        rc = time;
        if (rc == 0)
            rc = 1;
    }

    struct work *work = work_create(__deliver_alarm, current_task);

    schedule_work_delay(work, secs * 150);

    current_task->alarm_work = work;

    return rc;
}

int
sys_secsleep(int secs)
{
    sleep(secs * 150);

    if (current_task->flags & TFLAG_INTERRUPTED) {
        current_task->flags &= ~TFLAG_INTERRUPTED;
        printk("oops we got woken up, ret eintr\n");
        return -EINTR;
    }

    printk("we have successfully slept enough\n");

    return 0;
}

int
sys_gettimeofday(struct timeval *tv, void *z)
{
    return gettimeofday(tv, z);
}

struct mmap_arg_struct {
    void *addr;
    size_t len;
    int prot;
    int flags;
    int fd;
    size_t offset;
};

int
sys_mmap(struct mmap_arg_struct *arg)
{
    struct file *f = NULL;

    if (verify_buffer(arg, sizeof(*arg)))
        return -EFAULT;

    if (!(arg->flags & MAP_ANONYMOUS)) {
        if (arg->fd < 0 || arg->fd >= FD_MAX)
            return -EBADF;

        f = current_task->file_table[arg->fd];
        if (!f)
            return -EBADF;
    }

    return (int) do_mmap(arg->addr, arg->len, arg->prot, arg->flags, f, arg->offset);
}

int
sys_munmap(void *addr, size_t len)
{
    return -ENOSYS;
}

int
sys_getppid()
{
    return current_task->ppid;
}

int
sys_sigprocmask(int how, unsigned long *new, unsigned long *old)
{
    uint64_t old_mask = signal_get_mask(current_task);

    if (new == NULL)
        goto set_old;

    if (how == SIG_BLOCK) {
        signal_set_mask(current_task, old_mask | (uint32_t)*new);
        goto set_old;
    } else if (how == SIG_UNBLOCK) {
        signal_set_mask(current_task, old_mask & ~((uint32_t)*new));
        goto set_old;
    } else if (how == SIG_SETMASK) {
        signal_set_mask(current_task, (uint32_t) *new);
        goto set_old;
    }

    return -EINVAL;

set_old:
        //reschedule_to(current_task);
        if (old == NULL) {
            return 0;
        } else if (verify_buffer(old, sizeof(unsigned long)))
            return -EFAULT;

        *old = (uint32_t) old_mask;
        return 0;
}


void
__execve_dump_array(char **argv)
{
    int argc = 0;

    while (argv[argc ++])
        ;

    printk("{ ");
    for (int i = 0; i < argc; i ++)
        printk("%s%s", argv[i], i == argc - 1 ? "" : ", ");
    printk("}");
}

void
__trace_syscall(int no, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    int pid = current_task->pid;
    //printk("syscall from 0x%x: ", current_task->regs->eip);
    switch(no) {
        case 0x1:
            printk("pid %d sys_exit(%d)\n", pid, a);
            return;
        case 0x2:
            printk("pid %d sys_fork()\n", pid);
            return;
        case 0x3:
            printk("pid %d sys_read(%d, 0x%x, %d)\n", pid, a, b, c);
            return;
        case 0x4:
            printk("pid %d sys_write(%d, 0x%x, %d)\n", pid, a, b, c);
            return;
        case 0x5:
            printk("pid %d sys_open(%s, %s%s%s%s%s%s%s0x%x, 0x%x)\n", pid, a,
                    b & O_WRONLY ? "O_WRONLY | " : "",
                    b & O_RDWR ?   "O_RDWR | "   : "",
                    b & O_EXCL ?   "O_EXCL | "   : "",
                    b & O_TRUNC ? "O_TRUNC | " : "",
                    b & O_CREAT ? "O_CREAT | "   : "",
                    b & O_CLOEXEC ? " O_CLOEXEC | " : "",
                    b & O_NOCTTY ? "O_NOCTTY | " : "",
                    b & ~(O_TRUNC | O_WRONLY | O_RDWR | O_EXCL | O_CREAT | O_CLOEXEC), c);
            return;
        case 0x6:
            printk("pid %d sys_close(%d)\n", pid, a);
            return;
        case 0x7:
            printk("pid %d sys_waitpid(%d, 0x%x, %d)\n", pid, a, b, c);
            return;
        case 0xB:
            printk("pid %d sys_execve(%s, ", pid, a);
            __execve_dump_array(b);
            printk(", ");
            __execve_dump_array(c);
            printk(")\n");
            return;
        case 0xC:
            printk("pid %d sys_chdir(%s)\n", pid, a);
            return;
        case 0x11:
            printk("pid %d sys_socket(%d, %d, %d)\n", pid, a, b, c);
            return;
        case 0x12:
            printk("pid %d sys_stat(%s, 0x%x)\n", pid, a, b);
            return;
        case 0x13:
            printk("pid %d sys_lseek(%d, %d, %d)\n", pid, a, b, c);
            return;
        case 0x14:
            printk("pid %d sys_getpid()\n", pid);
            return;
        case 0x1b:
            printk("pid %d sys_alarm(%d)\n", pid, a);
            return;
        case 0x1c:
            printk("pid %d sys_fstat(%d, 0x%x)\n", pid, a, b);
            return;
        case 0x1f:
            printk("pid %d sys_connect(%d, 0x%x, %d)\n", pid, a, b, c);
            return;
        case 0x23:
            printk("pid %d sys_sbrk(0x%x)\n", pid, a);
            return;
        case 0x25:
            printk("pid %d sys_kill(%d, %s)\n", pid, a, signal_to_string(b));
            return;
        case 0x27:
            printk("pid %d sys_mkdir(%s, %d)\n", pid, a, b);
            return;
        case 0x29:
            printk("pid %d sys_dup(%d)\n", pid, a);
            return;
        case 0x2a:
            printk("pid %d sys_pipe(0x%x)\n", pid, a);
            return;
        case 0x2c:
            printk("pid %d sys_sysconf(%d)\n", pid, a);
            return;
        case 0x30:
            printk("pid %d sys_signal(%s, 0x%x)\n", pid, signal_to_string(a), b);
            return;
        case 0x36:
            printk("pid %d sys_ioctl(0x%x, 0x%x, 0x%x)\n", pid, a, b, c);return -1;
            return;
        case 0x37:
            printk("pid %d sys_fcntl(0x%x, 0x%x, 0x%x)\n", pid, a, b, c);
            return;
        case 0x39:
            printk("pid %d sys_setpgid(%d, %d)\n", pid, a, b);
            return;
        case 0x3f:
            printk("pid %d sys_dup2(%d, %d)\n", pid, a, b);
            return;
        case 0x40:
            printk("pid %d sys_getppid()\n", pid);
            return;
        case 0x42:
            printk("pid %d sys_setsid()\n", pid);
            return;
        case 0x4e:
            printk("pid %d sys_gettimeofday(0x%x, 0x%x)\n", pid, a, b);
            return;
        case 0x59:
            printk("pid %d sys_readdir(%d, 0x%x, %d)\n", pid, a, b, c);
            return;
        case 0x5a:
            printk("pid %d sys_mmap(UNIMPL)\n", pid);
            return;
        case 0x5b:
            printk("pid %d sys_munmap(0x%x, 0x%x)\n", pid, a, b);
            return;
        case 0x6d:
            printk("pid %d sys_uname(0x%x)\n", pid, a);
            return;
        case 0x7e:
            printk("pid %d sys_sigprocmask(%d, 0x%x, 0x%x)\n", pid, a, b, c);
            return;
        case 0x84:
            printk("pid %d sys_getpgid(%d)\n", pid, a);
            return;
        case 0xa2:
            printk("pid %d sys_secsleep(%d)\n", pid, a);
            return;
        case 0xb7:
            printk("pid %d sys_getcwd(0x%x, %d)\n", pid, a, b);
            return;
    }
}

void
__trace_return(int rc)
{
    printk("pid %d                                ---- rc: %d (%s)\n",
            current_task->pid, rc, errno_to_string(rc));
}

static int __sysctl_trace_sys = 0;

int
syscall_hub(int no, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    //printk("SYSCALL pid %d no dec:%d (hex:0x%x) a 0x%x b 0x%x c 0x%x d 0x%x\n",
            //current_task->pid, no, no, a, b, c, d);

    if (!signal_processing(current_task))
        current_task->flags |= TFLAG_PROCESSING_SYSCALL;

    if (__sysctl_trace_sys)
        __trace_syscall(no, a, b, c, d);
    int rc;
    switch(no) {
        case 0x1:
            sys_exit((int) a);
            __not_reached();
        case 0x2:
            rc = sys_fork();
            break;
        case 0x3:
            rc = sys_read((int) a, (char *)b, (size_t) c);
            break;
        case 0x4:
            rc = sys_write((int) a, (char *)b, (size_t) c);
            break;
        case 0x5:
            rc = sys_open((char *) a, (int) b, (int) c);
            break;
        case 0x6:
            rc = sys_close((int) a);
            break;
        case 0x7:
            rc = sys_waitpid((pid_t) a, (int *) b, (int) c);
            break;
        case 0xB:
            rc = sys_execve((char *) a, (char **) b, (char **) c);
            break;
        case 0xC:
            rc = sys_chdir((char *) a);
            break;
        case 0x11:
            rc = sys_socket((int) a, (int) b, (int) c);
            break;
        case 0x12:
            rc = sys_stat((char *) a, (struct stat *) b);
            break;
        case 0x13:
            rc = sys_lseek((int) a, (size_t) b, (int) c);
            break;
        case 0x14:
            rc = sys_getpid();
            break;
        case 0x1b:
            rc = sys_alarm((int) a);
            break;
        case 0x1c:
            rc = sys_fstat((int) a, (struct stat *) b);
            break;
        case 0x1f:
            rc = sys_connect((int) a, (void *) b, (size_t) c);
            break;
        case 0x23:
            rc = sys_sbrk((int) a);
            break;
        case 0x25:
            rc = sys_kill((int) a, (int) b);
            break;
        case 0x27:
            rc = sys_mkdir((char *) a, (int) b);
            break;
        case 0x29:
            rc = sys_dup((int) a);
            break;
        case 0x2a:
            rc = sys_pipe((int *) a);
            break;
        case 0x2c:
            rc = sys_sysconf((int) a);
            break;
        case 0x30:
            rc = sys_signal((int) a, (sighandler_t) b);
            break;
        case 0x36:
            rc = sys_ioctl((int) a, (unsigned int) b, (unsigned int) c);
            break;
        case 0x37:
            rc = sys_fcntl((int) a, (int) b, (int) c);
            break;
        case 0x39:
            rc = sys_setpgid((int) a, (int) b);
            break;
        case 0x3f:
            rc = sys_dup2((int) a, (int) b);
            break;
        case 0x40:
            rc = sys_getppid();
            break;
        case 0x42:
            rc = sys_setsid();
            break;
        case 0x4e:
            rc = sys_gettimeofday((void *) a, (void *) b);
        case 0x59:
            rc = sys_readdir((int) a, (struct linux_dirent *) b, (int) c);
            break;
        case 0x5a:
            rc = sys_mmap((void *) a);
            break;
        case 0x5b:
            rc = sys_munmap((unsigned long) a, (size_t) b);
            break;
        case 0x6d:
            rc = sys_uname((struct uname *) a);
            break;
        case 0x7e:
            rc = sys_sigprocmask((int) a, (void *) b, (void *)c);
            break;
        case 0x84:
            rc = sys_getpgid((int) a);
            break;
        case 0xa2:
            rc = sys_secsleep((int) a);
            break;
        case 0xb7:
            rc = sys_getcwd((char *) a, (unsigned long) b);
            break;
        default:
            syscall_undefined(no);
            rc = -ENOSYS;
            break;
    }

    if (__sysctl_trace_sys)
        __trace_return(rc);

    if (rc == -ENOENT) {
        //printk("SYSCALL:ENOENTd pid %d no dec:%d (hex:0x%x) a 0x%x b 0x%x c 0x%x d 0x%x\n",
            //current_task->pid, no, no, a, b, c, d);
    }

    __flush_tlb();

    if (!signal_processing(current_task))
        current_task->flags &= ~TFLAG_PROCESSING_SYSCALL;

    //printk("result: %d (%s)\n", rc, rc >= 0 ? "success" : errno_to_string(rc));
    return rc;
}
