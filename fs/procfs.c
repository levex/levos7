#include <levos/fs.h>
#include <levos/kernel.h>
#include <levos/list.h>
#include <levos/task.h>
#include <levos/tty.h>

static size_t
generic_write_buf(int pos, void *buf, size_t len, char *buffer)
{
    if (pos > strlen(buffer))
        return 0;

    if (pos + len > strlen(buffer))
        len = strlen(buffer) - pos;

    memcpy(buf, buffer + pos, len);

    return len;
}

static size_t
proc_uptime(int pos, void *buf, size_t len, char *buffer)
{
    char uptime_buf[32];

    memset(uptime_buf, 0, 32);

    itoa((uint32_t) time_get_uptime(), 10, uptime_buf);

    uptime_buf[strlen(uptime_buf)] = '\n';
    uptime_buf[strlen(uptime_buf)] = '\0';

    return generic_write_buf(pos, buf, len, uptime_buf);
}

struct procfs_file {
    int inode_no;
    char *path;
    size_t (*write_buf)(int, void *, size_t, char *);
    char *arg;
};

static char procfs_version[] = "LevOS 7.0 compiled on " __DATE__ " at " __TIME__ "\n";
static char procfs_hostname[] = "localhost\n";
extern size_t palloc_proc_memfree(int, void *, size_t, char *);
extern size_t palloc_proc_memused(int, void *, size_t, char *);
extern size_t palloc_proc_memtotal(int, void *, size_t, char *);
extern size_t heap_proc_heapstats(int, void *, size_t, char *);

static struct procfs_file _files[] = {
    { 0x80000001, "/version", generic_write_buf, procfs_version},
    { 0x80000002, "/hostname", generic_write_buf, procfs_hostname},
    { 0x80000003, "/memfree", palloc_proc_memfree, NULL},
    { 0x80000004, "/memused", palloc_proc_memused, NULL},
    { 0x80000005, "/memtotal", palloc_proc_memtotal, NULL},
    { 0x80000006, "/heapstats", heap_proc_heapstats, NULL},
    { 0x80000007, "/uptime", proc_uptime, NULL},
    { 0x00000000, NULL, NULL},
};

int
procfs_get_inode(char *path)
{
    struct procfs_file *f = &_files[0];
    int i = 0;

    /* check for root files */
    for (i = 0, f = &_files[i]; f && f->path != NULL; f = &_files[++i]) {
        if (strcmp(path, f->path) == 0)
            return f->inode_no;
    }

    /* check for process files */
    int _pid = atoi_10(path + 1);
    if (get_task_for_pid(_pid))
        return _pid;

    return -1;
}

/* XXX: convert this to sprintf */
char *
procfs_create_process_dump(pid_t pid, size_t *_size) {
    struct task *task = get_task_for_pid(pid);
    char *buffer = malloc(4096);
    char *orig_buffer = buffer;
    char itoa_buffer[16];
    size_t size = 0;

    if (!buffer)
        return NULL;

    memset(buffer, 0, 4096);

    if (!task) {
        free(buffer);
        return NULL;
    }

#define WRITE_STRING(str, len) memcpy(buffer, str, len); buffer += len; size += len;
#define WRITE_INT(val) itoa(val, 10, itoa_buffer); WRITE_STRING(itoa_buffer, strlen(itoa_buffer));
#define WRITE_NEWLINE WRITE_STRING("\n", 1);
    WRITE_STRING("pid ", 4);
    WRITE_INT(task->pid);
    WRITE_NEWLINE;
    WRITE_STRING("ppid ", 5);
    WRITE_INT(task->ppid);
    WRITE_NEWLINE;
    WRITE_STRING("pgid ", 5);
    WRITE_INT(task->pgid);
    WRITE_NEWLINE;
    WRITE_STRING("sid ", 4);
    WRITE_INT(task->sid);
    WRITE_NEWLINE;
    WRITE_STRING("comm ", 5);
    if (task->comm) {
        WRITE_STRING(task->comm, strlen(task->comm));
    } else {
        WRITE_STRING("NULL", 4);
    }
    WRITE_NEWLINE;
    WRITE_STRING("cwd ", 4);
    if (task->cwd) {
        WRITE_STRING(task->cwd, strlen(task->cwd));
    } else {
        WRITE_STRING("NULL", 4);
    }
    WRITE_NEWLINE;
    if (task->ctty) {
        WRITE_STRING("ctty /dev/tty", 13);
        WRITE_INT(task->ctty->tty_id);
        WRITE_NEWLINE;
    }


    *_size = size;

    return orig_buffer;
}

size_t procfs_read(struct file *filp, void *buf, size_t len)
{
    struct procfs_file *f = &_files[0];
    int i = 0;

    if (filp->priv > 0x80000000) {
        for (i = 0, f = &_files[i]; f && f->path != NULL; f = &_files[++i]) {
            if (strcmp(filp->respath, f->path) == 0) {
                int rc = f->write_buf(filp->fpos, buf, len, f->arg);
                filp->fpos += rc;
                return rc;
            }
        }
    } else {
        size_t size;
        /* construct the description */
        char *dump = procfs_create_process_dump(filp->priv, &size);
        if (dump == NULL)
            return -ENOENT;

        if (filp->fpos >= size)
            return 0;

        if (len > size - filp->fpos)
            len = size - filp->fpos;

        memcpy(buf, dump + filp->fpos, len);
        filp->fpos += len;

        free(dump);
        return len;
    }

    return -ENOENT;
}

size_t
procfs_write(struct file *filp, void *buf, size_t len)
{
    return -EROFS;
}
int
procfs_fstat(struct file *filp, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int
procfs_close(struct file *filp)
{
    free(filp->respath);
    free(filp);
    return 0;
}

int
procfs_readdir(struct file *filp, struct linux_dirent *de)
{
    struct procfs_file *f = &_files[0];
    int i = 0, success = 0;
    struct task *task = NULL;
    struct list_elem *elem;
    char itoa_buf[12];
    extern struct list *__ALL_TASKS_PTR;

    memset(de->d_name, 0, 255);

    if (filp->fpos == 0) {
        success = 1;
        de->d_ino = 0;
        de->d_off = 0;
        memcpy(de->d_name, ".", 1);
        de->d_reclen = 11;
        filp->fpos ++;
        goto done;
    }

    if (filp->fpos == 1) {
        success = 1;
        de->d_ino = 0;
        de->d_off = 0;
        memcpy(de->d_name, "..", 2);
        de->d_reclen = 12;
        filp->fpos ++;
        goto done;
    }

    i = 2;

    memset(itoa_buf, 0, 12);

    /* start with the running processes */
    list_foreach_raw(__ALL_TASKS_PTR, elem) {
        task = list_entry(elem, struct task, all_elem);
        if (i == filp->fpos) {
            success = 1;
            de->d_ino = task->pid;
            de->d_off = 0;
            itoa(task->pid, 10, itoa_buf);
            memcpy(de->d_name, itoa_buf, strlen(itoa_buf));
            de->d_reclen = 10 + strlen(itoa_buf);
            filp->fpos ++;
            goto done;
        }
        i ++;
    }

    while (f->path) {
        if (i == filp->fpos) {
            success = 1;
            de->d_ino = f->inode_no;
            de->d_off = 0;
            memcpy(de->d_name, f->path + 1, strlen(f->path));
            de->d_reclen = 10 + strlen(f->path);
            filp->fpos ++;
            break;
        }
        f ++;
        i ++;
    }

done:
    if (!success)
        return -ENOSPC;

    return 0;
}

int
procfs_ioctl(struct file *filp, unsigned long req, unsigned long arg)
{
    return -EINVAL;
}

struct file_operations procfs_fops = {
    .read = procfs_read,
    .write = procfs_write,
    .fstat = procfs_fstat,
    .close = procfs_close,
    .readdir = procfs_readdir,
};

struct file *
procfs_open_root(struct filesystem *fs, struct file *filp)
{
    filp->isdir = 1;
    filp->type = FILE_TYPE_NORMAL;
    filp->priv = 0x80000000;
    return filp;
}

struct file *
procfs_open(struct filesystem *fs, char *path)
{
    struct file *filp;
    int inode;

    inode = procfs_get_inode(path);
    if (inode == -1)
        return -ENOENT;
    
    filp = malloc(sizeof(*filp));
    if (!filp)
        return NULL;

    filp->fops = &procfs_fops;
    filp->fs = fs;
    filp->fpos = 0;
    filp->priv = NULL;
    filp->isdir = 0;
    filp->refc = 1;
    filp->respath = strdup(path);
    filp->type = FILE_TYPE_NORMAL;
    filp->priv = inode;

    //printk("%s: %s\n", __func__, path);
    
    if (strcmp(filp->respath, "/") == 0 || strlen(filp->respath) == 0)
        return procfs_open_root(fs, filp);

    return filp;
}

int
procfs_stat(struct filesystem *fs, char *path, struct stat *st)
{
    if (strcmp(path, "/") == 0 || strlen(path) == 0)
        st->st_mode = S_IFDIR;

    return 0;
}

struct file *
procfs_create(struct filesystem *fs, char *path)
{
    return ERR_PTR(-EROFS);
}

extern struct fs_ops procfs_fs_ops;

struct filesystem *
procfs_mount(struct device *dev)
{
    struct filesystem *fs = malloc(sizeof(*fs));
    if (!fs)
        return NULL;

    if (dev != (void *) 0)
        return -ENODEV;

    //dev->fs = fs;
    fs->priv_data = NULL;
    fs->fs_ops = &procfs_fs_ops;
    fs->dev = dev;

    printk("profs: mounted\n");

    return fs;
}

struct fs_ops procfs_fs_ops = {
    .fsname = "procfs",
    .open = procfs_open,
    .stat = procfs_stat,
    .create = procfs_create,
    .mount = procfs_mount,
};

int
procfs_init()
{
    struct fs_ops *f = malloc(sizeof(*f));
    memcpy(f, (void *) &procfs_fs_ops, sizeof(*f));
    register_fs(f);
    return 0;
}
