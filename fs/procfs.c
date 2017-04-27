#include <levos/fs.h>
#include <levos/kernel.h>

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

struct procfs_file {
    char *path;
    size_t (*write_buf)(int, void *, size_t, char *);
    char *arg;
};

static char procfs_version[] = "LevOS 7.0 compiled on " __DATE__ " at " __TIME__ "\n";
static char procfs_hostname[] = "localhost\n";
extern size_t palloc_proc_memfree(int, void *, size_t, char *);
extern size_t palloc_proc_memused(int, void *, size_t, char *);
extern size_t palloc_proc_memtotal(int, void *, size_t, char *);

static struct procfs_file _files[] = {
    { "/version", generic_write_buf, procfs_version},
    { "/hostname", generic_write_buf, procfs_hostname},
    { "/memfree", palloc_proc_memfree, NULL},
    { "/memused", palloc_proc_memused, NULL},
    { "/memtotal", palloc_proc_memused, NULL},
    { NULL, NULL},
};

size_t procfs_read(struct file *filp, void *buf, size_t len)
{
    struct procfs_file *f = &_files[0];
    int i = 0;

    for (i = 0, f = &_files[i]; f && f->path != NULL; f = &_files[++i]) {
        if (strcmp(filp->respath, f->path) == 0) {
            int rc = f->write_buf(filp->fpos, buf, len, f->arg);
            filp->fpos += rc;
            return rc;
        }
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
    free(filp);
    return 0;
}

int
procfs_readdir(struct file *filp, struct linux_dirent *de)
{
    struct procfs_file *f = &_files[0];
    int i = 0, success = 0;

    while (f->path) {
        if (i == filp->fpos) {
            success = 1;
            de->d_ino = 0;
            de->d_off = 0;
            memcpy(de->d_name, f->path + 1, strlen(f->path));
            de->d_reclen = 10 + strlen(f->path);
            filp->fpos ++;
            break;
        }
        f ++;
        i ++;
    }

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
    return filp;
}

struct file *
procfs_open(struct filesystem *fs, char *path)
{
    struct file *filp = malloc(sizeof(*filp));
    if (!filp)
        return NULL;

    filp->fops = &procfs_fops;
    filp->fs = fs;
    filp->fpos = 0;
    filp->priv = NULL;
    filp->isdir = 0;
    filp->refc = 1;
    filp->respath = path;
    filp->type = FILE_TYPE_NORMAL;
    
    if (strcmp(path, "/") == 0)
        return procfs_open_root(fs, filp);
}

int
procfs_stat(struct filesystem *fs, char *path, struct stat *st)
{
    //memset(st, 0, sizeof(*st));
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
