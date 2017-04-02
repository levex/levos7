#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/device.h>
#include <levos/ext2.h>

struct filesystem *ext2_mount(struct device *dev);
int ext2_stat(struct filesystem *, char *, struct stat *);

struct fs_ops ext2_fs = {
    .fsname = "ext2",
    .mount = ext2_mount,
    .open = ext2_open,
    .stat = ext2_stat,
};

struct filesystem *ext2_mount(struct device *dev)
{
    if (dev->type != DEV_TYPE_BLOCK)
        return (void *) -ENODEV;

    void *buf = malloc(1024);
    dev_seek(dev, 2);
    int i = dev->read(dev, buf, 2);
    struct ext2_superblock *sb = (struct ext2_superblock *)buf;
    if (sb->ext2_sig != EXT2_SIGNATURE) {
        printk("ext2: signature mismatch (0x%x) on device %s\n", sb->ext2_sig, dev->name);
        return NULL;
    }
    struct ext2_priv_data *p = malloc(sizeof(*p));
    struct filesystem *fs = malloc(sizeof(*fs));
    p->blocksize = 1024 << sb->blocksize_hint;
    p->inodes_per_block = p->blocksize / sizeof(struct ext2_inode);
    p->sectors_per_block = p->blocksize / 512;
    printk("ext2: %s: volume size: %d bytes\n", dev->name, p->blocksize * sb->blocks);
    uint32_t number_of_bgs0 = sb->blocks / sb->blocks_in_blockgroup;
    if (!number_of_bgs0) number_of_bgs0 = 1;
    p->number_of_bgs = number_of_bgs0;

    p->first_bgd = sb->superblock_id + sizeof(struct ext2_superblock) / p->blocksize;
    memcpy(&p->sb, (void *) sb, sizeof(struct ext2_superblock));


    dev->fs = fs;
    fs->priv_data = p;
    fs->fs_ops = &ext2_fs;
    fs->dev = dev;

    return fs;
}

int
ext2_write_superblock(struct filesystem *fs)
{
    void *buffer = &EXT2_PRIV(fs)->sb;
    dev_seek(fs->dev, 2);
    if (!fs->dev->write) {
        printk("[ext2]: CRIRTICAL: ROFS\n");
        return -EROFS;
    }
    fs->dev->write(fs->dev, buffer, 2);
    printk("[ext2]: WARNING: writing superblock has been done\n");
}

int ext2_init()
{
    printk("ext2: registered filesystem to vfs\n");
    struct fs_ops *f = malloc(sizeof(*f));
    memcpy(f, (void *) &ext2_fs, sizeof(*f));
    register_fs(f);
    return 0;
}
