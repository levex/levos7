#ifndef __LEVOS_FS_H
#define __LEVOS_FS_H

#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/device.h>

struct filesystem;

/* a mount is a filesystem (@fs) mounted on device @dev at point @point */
struct mount {
    char *point;
    struct filesystem *fs;
    struct device *dev;
};

struct file;
struct stat;

struct linux_dirent {
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    char           d_name[255];
};

struct file_operations {
    size_t (*read)(struct file *, void *, size_t);
    size_t (*write)(struct file *, void *, size_t);
    int (*fstat)(struct file *, struct stat *);
    int (*close)(struct file *);
    int (*readdir)(struct file *, struct linux_dirent *);
    int (*ioctl)(struct file *, unsigned long, unsigned long arg);
};

#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_APPEND  0x0008
#define O_CREAT   0x0200
#define O_TRUNC   0x0400
#define O_EXCL    0x0800
#define O_SYNC    0x2000

#define FILE_TYPE_NORMAL 0
#define FILE_TYPE_SOCKET 1
#define FILE_TYPE_TTY    2
#define FILE_TYPE_PIPE   3

struct file {
    struct file_operations *fops;
    struct filesystem *fs;
    int fpos;
    int isdir;
    int type;
    int refc;
    char *respath;
    void *priv;
};

struct stat {
    uint16_t st_dev;
    uint16_t st_ino;
    uint32_t st_mode;
    uint16_t st_nlink;
    uint16_t st_uid;
    uint16_t st_gid;
    uint16_t st_rdev;
    uint32_t st_size;
    /* 32*/
};

struct fs_ops {
    char *fsname;
    struct file *(*open)(struct filesystem *, char *);
    int (*stat)(struct filesystem *, char *, struct stat *);
    struct file *(*create)(struct filesystem *, char *);
    struct filesystem *(*mount)(struct device *);
};

/* a filesystem */
struct filesystem {
    void *priv_data;
    struct device *dev;
    struct fs_ops *fs_ops;
};

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define S_IFCHR  0020000

void file_seek(struct file *, int);
int vfs_init(void);
int vfs_mount(char *, struct device *);
int register_fs(struct fs_ops *fs);
struct file *vfs_open(char *);
int vfs_stat(char *, struct stat *);
struct file *dup_file(struct file *);
struct file *vfs_create(char *);
void vfs_close(struct file *);

/* path manipulation stuff */
inline char *
__path_get_path(char *path)
{
    int len = strlen(path), i, last_idx = 0;
    char *ret;

    for (i = 0; i < len; i ++)
        if (path[i] == '/')
            last_idx = i;

    if (last_idx == 0)
        return strdup("/");

    ret = malloc(last_idx + 2);
    if (!ret)
        return NULL;

    memset(ret, 0, last_idx + 2);

    memcpy(ret, path, last_idx);
    ret[last_idx + 1] = 0;
    return ret;
}

inline void
__path_free(char *path)
{
    free(path);
}

inline char *
__path_get_name(char *path)
{
    int len = strlen(path);
    if (path[len] == '/')
        len --;

    while (path[len] != '/')
        len --;

    return &path[++ len];
}

#endif /* __LEVOS_FS_H */
