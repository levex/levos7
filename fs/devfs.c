#include <levos/fs.h>
#include <levos/kernel.h>
#include <levos/list.h>
#include <levos/task.h>
#include <levos/tty.h>

#define MODULE_NAME devfs

int urandom_file_read(struct file *f, void *_buf, size_t count)
{
    char *buf = _buf;
    int a = count;
    char *abc = "gsdh$$l4gt2@@75b34j^&$!k5n348rg3fu3h47tg54ihjkn4gjfsdl";

    for (int i = 0; i < count; i ++)
        buf[i] = abc[i % strlen(abc)];

    return a;
}

int urandom_file_write(struct file *f, void *_buf, size_t count)
{
    return -EPERM;
}

int urandom_file_fstat(struct file *f, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int
urandom_file_close(struct file *f)
{
    free(f->full_path);
    free(f);
}

struct file_operations urandom_fops = {
    .read = urandom_file_read,
    .write = urandom_file_write,
    .fstat = urandom_file_fstat,
    .close = urandom_file_close,
};

struct file urandom_base_file = {
    .fops = &urandom_fops,
    .fs = NULL,
    .fpos = 0,
    .isdir = 0,
    .respath = "urandom",
    .full_path = "/dev/urandom",
};

struct devfs_file {
    int inode_no;
    char *path;
    struct file *base_file;
};


int null_file_write(struct file *f, void *_buf, size_t count)
{
    return count;
}

int null_file_read(struct file *f, void *_buf, size_t count)
{
    return 0;
}

int null_file_fstat(struct file *f, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int
null_file_truncate(struct file *f, int pos)
{
    return 0;
}

int
null_file_close(struct file *f)
{
    free(f->full_path);
    free(f);
}

struct file_operations null_fops = {
    .read = null_file_read,
    .write = null_file_write,
    .fstat = null_file_fstat,
    .close = null_file_close,
    .truncate = null_file_truncate,
};

struct file null_base_file = {
    .fops = &null_fops,
    .fs = NULL,
    .fpos = 0,
    .isdir = 0,
    .respath = "null",
    .full_path = "/dev/null",
};

int zero_file_write(struct file *f, void *_buf, size_t count)
{
    return 0;
}

int zero_file_read(struct file *f, void *_buf, size_t count)
{
    memset(_buf, 0, count);

    return count;
}

int zero_file_fstat(struct file *f, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int
zero_file_truncate(struct file *f, int pos)
{
    return 0;
}

int
zero_file_close(struct file *f)
{
    free(f->full_path);
    free(f);
}

struct file_operations zero_fops = {
    .read = zero_file_read,
    .write = zero_file_write,
    .fstat = zero_file_fstat,
    .close = zero_file_close,
    .truncate = zero_file_truncate,
};

struct file zero_base_file = {
    .fops = &zero_fops,
    .fs = NULL,
    .fpos = 0,
    .isdir = 0,
    .respath = "zero",
    .full_path = "/dev/zero",
};

extern struct file kbd_base_file;
extern struct file serial_base_file;
extern struct file fb_base_file;
extern struct file ctty_base_file;


static struct devfs_file _files[] = {
    { 0x80000001, "/urandom", &urandom_base_file},
    { 0x80000001, "/random", &urandom_base_file},
    { 0x80000002, "/null", &null_base_file},
    { 0x80000003, "/zero", &zero_base_file},
    { 0x80000004, "/kbd", &kbd_base_file},
    { 0x80000005, "/ttyS0", &serial_base_file},
    { 0x00000006, "/tty", &ctty_base_file},
    { 0x00000007, "/fb", &fb_base_file},
    { 0x00000000, NULL, NULL},
};

static int _files_num = (sizeof(_files) / sizeof(_files[0]));

int
devfs_get_inode(char *path, struct file **filp)
{
    struct devfs_file *f = &_files[0];
    int i = 0;

    /* check for root files */
    for (i = 0, f = &_files[i]; f && f->path != NULL; f = &_files[++i]) {
        if (strcmp(path, f->path) == 0) {
            if (filp)
                *filp = f->base_file;
            return f->inode_no;
        }
    }

    if (strcmp(path, "/") == 0) {
        *filp = &null_base_file;
        return 0x80000000;
    }

    return -1;
}

int
devfs_readdir(struct file *filp, struct linux_dirent *de)
{
    struct devfs_file *f = &_files[0];
    int i = 0, success = 0;

    //mprintk("readdir: pos %d\n", filp->fpos);
    memset(de->d_name, 0, 255);
    
    if (filp->fpos == 0) {
        success = 1;
        de->d_ino = 0;
        de->d_off = 0;
        memcpy(de->d_name, ".", 1);
        de->d_reclen = 10 + 1;
        filp->fpos ++;
        goto done;
    }

    if (filp->fpos == 1) {
        success = 1;
        de->d_ino = 0;
        de->d_off = 0;
        memcpy(de->d_name, "..", 2);
        de->d_reclen = 10 + 2;
        filp->fpos ++;
        goto done;
    }
    
    i = 2;

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

struct file *
devfs_open_root(struct filesystem *fs, struct file *filp)
{
    filp->isdir = 1;
    filp->type = FILE_TYPE_NORMAL;
    filp->priv = 0x80000000;
    filp->fs = fs;
    filp->fops->readdir = devfs_readdir;
    return filp;
}

struct file *
devfs_open(struct filesystem *fs, char *path)
{
    struct file *filp;
    int inode;

    inode = devfs_get_inode(path, &filp);
    if (inode == -1)
        return -ENOENT;

    filp = dup_file(filp);
    
    if (strcmp(path, "/") == 0 || strlen(path) == 0)
        return devfs_open_root(fs, filp);

    return filp;
}

int
devfs_stat(struct filesystem *fs, char *path, struct stat *st)
{
    if (strcmp(path, "/") == 0 || strlen(path) == 0)
        st->st_mode = S_IFDIR;
    else if (devfs_get_inode(path, NULL) == -1)
        return -ENOENT;

    return 0;
}

struct file *
devfs_create(struct filesystem *fs, char *path)
{
    return ERR_PTR(-EROFS);
}

extern struct fs_ops devfs_fs_ops;

struct filesystem *
devfs_mount(struct device *dev)
{
    struct filesystem *fs = malloc(sizeof(*fs));
    if (!fs)
        return NULL;

    if (dev != (void *)1) {
        free(fs);
        return -ENODEV;
    }

    //dev->fs = fs;
    fs->priv_data = NULL;
    fs->fs_ops = &devfs_fs_ops;
    fs->dev = dev;

    printk("profs: mounted\n");

    return fs;
}

struct fs_ops devfs_fs_ops = {
    .fsname = "devfs",
    .open = devfs_open,
    .stat = devfs_stat,
    .create = devfs_create,
    .mount = devfs_mount,
};

int
devfs_init()
{
    struct fs_ops *f = malloc(sizeof(*f));
    memcpy(f, (void *) &devfs_fs_ops, sizeof(*f));
    register_fs(f);
    return 0;
}
