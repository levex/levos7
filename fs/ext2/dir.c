#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>

int ext2_read_directory(struct filesystem *fs, int dino, char *f)
{
    /* read the directory inode in */
    struct ext2_inode *inode = malloc(sizeof(*inode));
    ext2_read_inode(fs, inode, dino);

    if (inode->type & 0x4000 == 0)
        return -ENOTDIR;

    /* the block pointers contain some 'struct ext2_dir's, so parse */
    void *bbuf = malloc(EXT2_PRIV(fs)->blocksize);
    for (int i = 0; i < 12; i++) {
        ext2_read_block(fs, bbuf, inode->dbp[i]);
        struct ext2_dir *d = (void *)bbuf;
        if (d->size == 0 || d->namelength == 0)
            break;
        int r = 0;
        while (r < EXT2_PRIV(fs)->blocksize) {
            if (strncmp(&d->reserved + 1, f, d->namelength) == 0) {
                int k = d->inode;
                free(bbuf);
                return k;
            }
            r += d->size;
            if (d->size == 0 || d->namelength == 0) {
                goto c1;
            }
            d = (struct ext2_dir *)((uintptr_t)d + d->size);
        }
        c1:;
    }
    free(bbuf);
    return -ENOENT;
}
