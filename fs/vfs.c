#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>
#include <levos/string.h>

#define MAX_MOUNTS 256

struct mount *mounts[MAX_MOUNTS];
int nmounts = 0;

struct mount *root_mount = 0;

struct fs_ops *fs_ops_s[8];
static int fs_ops_n = 0;

void
file_seek(struct file *file, int pos)
{
    file->fpos = pos;
}


/*
 * __check_mounts - Checks whether a path is already mounted
 *
 * @internal
 */
struct mount *__check_mounts(char *str)
{
    for (int i = 0; i < nmounts; i++)
        if (strcmp(mounts[i]->point, str) == 0)
            return mounts[i];

    return 0;
}

/*
 * __vfs_set_mount - Sets path to be mounted as a particular
 * filesystem on the device
 *
 * @internal
 */
int __vfs_set_mount(char *p, struct device *dev, struct filesystem *fs)
{
    struct mount *m = malloc(sizeof(*m));
    if (!m)
        return -ENOMEM;

    m->point = p;
    m->fs = fs;
    m->dev = dev;
    mounts[nmounts] = m;
    nmounts ++;
    printk("vfs: mounted %s on %s to %s\n", fs->fs_ops->fsname, dev->name, p);
    if (strcmp(p, "/") == 0)
        root_mount = m;
    return 0;
}

/*
 * find_mount - Finds the internal mount structure for
 * a particular path
 */
struct mount *find_mount(char *path)
{
    struct mount *ret = root_mount;
    char *str;

    /* create a copy of the string */
    str = malloc(strlen(path) + 1);
    memcpy(str, path, strlen(path) + 1);
    if (strcmp(str, "/") == 0)
        goto out;

    while (strlen(str) > 1) {
        ret = __check_mounts(str);
        if (ret)
            goto out;
        str[strlen(str) - 1] = 0;
    }

    ret = root_mount;
out:
    free(str);
    return ret;
}

/*
 * vfs_mount_fs - Finds a filesystem that is willing to be
 * mounted on this particular device.
 *
 * WARNING: Filesystem will do its mounting on the device,
 * possibly even modify it!
 *
 * @internal
 */
struct filesystem *vfs_mount_fs(struct device *dev)
{
    struct filesystem *fs = 0;
    for (int i = 0; i < fs_ops_n; i++)
    {
        if (fs_ops_s[i]->mount) {
            fs = fs_ops_s[i]->mount(dev);
            if (fs)
                return fs;
        }
    }
    return 0;
}

/*
 * vfs_mount - Mount a device on a path
 *
 * BUG: allows potential 'floating' mounts
 *
 */
int vfs_mount(char *path, struct device *dev)
{
    if (!root_mount && strcmp("/", path) != 0)
        return -EAGAIN;

    struct filesystem *fs = vfs_mount_fs(dev);
    if (!fs)
        return -ENXIO;

    return __vfs_set_mount(path, dev, fs);
}

/*
 * vfs_root_mounted - Returns whether root directory was mounted
 * or not
 */
int vfs_root_mounted()
{
    return (root_mount)?1:0;
}

/*
 * register_fs - Register a filesystem so that it can later probe and
 * mounted on devices.
 */
int register_fs(struct fs_ops *fs)
{
    if (!fs)
        return -EINVAL;

    printk("vfs: registered filesystem %s\n", fs->fsname);

    fs_ops_s[fs_ops_n ++] = fs;
    return 0;
}

/*
 * vfs_open - backend for FS-related open(2)
 */
struct file *vfs_open(char *path)
{
    struct mount *m = find_mount(path);
    return m->fs->fs_ops->open(m->fs, path);
}


struct file *
dup_file(struct file *f)
{
    struct file *ret = malloc(sizeof(*ret));
    if (!ret)
        return (void *) -ENOMEM;

    /* copy file operations */
    ret->fops = f->fops;
    /* they are on the same filesystem */
    ret->fs = f->fs;
    /* but it starts at zero */
    ret->fpos = 0;
    /* same statistics and other stuff */
    ret->isdir = f->isdir;
    ret->respath = strdup(f->respath);

    /* done */
    return ret;
}

/*
 * vfs_stat - backend for FS-related stat(2)
 */
int vfs_stat(char *p, struct stat *buf)
{
    struct mount *m = find_mount(p);
    if (!m)
        panic("Location %s was not mounted!\n", p);

    if (!m->fs)
        panic("Mount deciphered for %s had not fs\n", p);

    if (!m->fs->fs_ops->stat)
        return -ENOSYS;
    return m->fs->fs_ops->stat(m->fs, p, buf);
}

/*
 * vfs_init - Initialize the Virtual Filesystem Layer
 */
int vfs_init(void)
{
    printk("vfs: loading filesystems\n");
    fs_ops_n = 0;

    ext2_init();

    return 0;
}
