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

struct file_operations {
    size_t (*read)(struct file *, void *, size_t);
    size_t (*write)(struct file *, void *, size_t);
    int (*fstat)(struct file *, struct stat *);
    int (*close)(struct file *);
};

#define FILE_TYPE_NORMAL 0
#define FILE_TYPE_SOCKET 1

struct file {
    struct file_operations *fops;
    struct filesystem *fs;
    int fpos;
    int isdir;
    int type;
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
    struct filesystem *(*mount)(struct device *);
};

/* a filesystem */
struct filesystem {
    void *priv_data;
    struct device *dev;
    struct fs_ops *fs_ops;
};

#define S_IFCHR  0020000

void file_seek(struct file *, int);
int vfs_init(void);
int vfs_mount(char *, struct device *);
int register_fs(struct fs_ops *fs);
struct file *vfs_open(char *);
int vfs_stat(char *, struct stat *);
struct file *dup_file(struct file *);

#endif /* __LEVOS_FS_H */
