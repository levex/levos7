#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>

int
ext2_find_file_inode(struct filesystem *fs, char *path)
{
    char *fn = malloc(strlen(path) + 1);
    if (!fn)
        return -ENOMEM;
    memset(fn, 0, strlen(path) + 1);
    memcpy(fn, path, strlen(path) + 1);

    char *pch, *lasts;
    pch = strtok_r(fn, "/", &lasts);
    if (!pch) {
        /* user wants the R! (root directory) */
        return 2;
    }

    /* this means that the tokenizing has at least one token, so
     * let us find the inode number of that token */
    int ino = 2;
    while (pch != 0) {
        ino = ext2_read_directory(fs, ino, pch);
        if (ino < 0)
            return -ENOENT;
        pch = strtok_r(0, "/", &lasts);
    }
    return ino;
}

int
ext2_stat(struct filesystem *fs, char *p, struct stat *buf)
{
    if (!p || !buf || !fs)
        return -EINVAL;

    int inode = ext2_find_file_inode(fs, p);
    if (inode < 0)
        return inode;

    struct ext2_inode *ibuf = malloc(sizeof(*ibuf));
    if (!ibuf)
        return -ENOMEM;

    ext2_read_inode(fs, ibuf, inode);
    buf->st_dev = 0;
    buf->st_ino = inode;
    buf->st_mode = ibuf->type;
    buf->st_nlink = ibuf->hardlinks;
    buf->st_uid = ibuf->uid;
    buf->st_gid = ibuf->gid;
    buf->st_rdev = 0;
    buf->st_size = ibuf->size;

    return 0;
}

int ext2_file_fstat(struct file *f, struct stat *st)
{
    struct ext2_inode *ibuf = malloc(sizeof(*ibuf));
    int ino = ext2_find_file_inode(f->fs, f->respath);

    if (!ibuf)
        return -ENOMEM;

    ext2_read_inode(f->fs, ibuf, ino);
    st->st_dev = 0;
    st->st_ino = ino;
    st->st_mode = ibuf->type;
    st->st_nlink = ibuf->hardlinks;
    st->st_uid = ibuf->uid;
    st->st_gid = ibuf->gid;
    st->st_rdev = 0;
    st->st_size = ibuf->size;
}

int
ext2_file_read_block(struct file *f, void *buf, size_t b)
{
    struct filesystem *fs = f->fs;
    struct ext2_inode *ibuf = malloc(sizeof(*ibuf));
    int ino = ext2_find_file_inode(fs, f->respath);
    int bs = EXT2_PRIV(fs)->blocksize;
    uint32_t p = bs / sizeof(uint32_t);
    if (ino < 0)
        return ino;

    if (!ibuf)
        return -ENOMEM;

    ext2_read_inode(fs, ibuf, ino);

    if (b < 12) {
        ext2_read_block(fs, buf, ibuf->dbp[b]);
    } else {
        if (b < 12 + p) {
            uint32_t *bb = malloc(bs);
            if (!bb)
                return -ENOMEM;
            //printk("SLB: %d\n", ibuf->singly_block);
            ext2_read_block(fs, bb, ibuf->singly_block);
            //printk("ORIG %d\n", b);
            b = bb[b - 12];
            //printk("TRY %d\n", b);
            ext2_read_block(fs, buf, b);
            free(bb);
            goto exit;
        } else if (b < 12 + p + p * p) {
            int A = b - 12;
            int B = A - p;
            int C = B / p;
            int D = B - C * p;

            uint32_t *buf1 = malloc(bs);
            if (!buf1)
                return -ENOMEM;

            //printk("reading %d in the doubly block %d\n", b, ibuf->doubly_block);
            ext2_read_block(fs, buf1, ibuf->doubly_block);

            uint32_t nblock = buf1[C];
            //printk("reading in the refd block: %d from loc %d\n", nblock, C);
            ext2_read_block(fs, buf1, nblock);

            //printk("doubly: reading %d from loc %d\n", buf1[D], D);
            ext2_read_block(fs, buf, buf1[D]);

            free(buf1);
            goto exit;
        }
        printk("Triply block are not yet supported\n");
        return -ENOSYS;
    }

exit:    free(ibuf);
    return 0;
}

int
ext2_read_file(struct file *f, void *buf, size_t count)
{
    if (!count)
        return 0;

    if (!f || !buf)
        return -EINVAL;

    int inode = ext2_find_file_inode(f->fs, f->respath);
    if (inode < 0)
        return inode;

    int total;
    int bs = EXT2_PRIV(f->fs)->blocksize;

    /* determine how many blocks do we need */
    int blocks = count / bs;
    int off = count % bs;
    int rblocks = 0; /* blocks read so far */

    /* determine position */
    int cblock = f->fpos / bs;
    int coff = f->fpos % bs;

    void *block = malloc(bs * (blocks + 1));
    if (!block)
        return -ENOMEM;

    ext2_file_read_block(f, block, cblock);
    for (int i = 0; i < blocks; i++)
        ext2_file_read_block(f, block + (i + 1) * bs, cblock + i + 1);

    /* now read the bytes */
    memcpy(buf, block + (f->fpos - cblock * bs), count);

    f->fpos += count;

    free(block);
    return count;
}

size_t
ext2_write_file(struct file *f, void *buf, size_t count)
{
    return -EROFS;
}

struct file_operations ext2_fops = {
    .read = ext2_read_file,
    .write = ext2_write_file,
};

struct file *ext2_open(struct filesystem *fs, char *p)
{
    struct file *f;
    struct ext2_inode *inode;
    int ino;

    if (!fs || !p)
        return (void *) -EINVAL;

    f = malloc(sizeof(*f));
    if (!f)
        return (void *) -ENOMEM;

    inode = malloc(sizeof(*inode));
    if (!inode) {
        free(f);
        return (void *) -ENOMEM;
    }

    ino = ext2_find_file_inode(fs, p);
    if (ino == -ENOENT) {
        free(inode);
        free(f);
        return (void *) -ENOENT;
    }

    ext2_read_inode(fs, inode, ino);

    f->fops = &ext2_fops;
    f->fs = fs;
    f->respath = p;
    f->isdir = (inode->type & INODE_TYPE_DIRECTORY != 0);
    f->fpos = 0;

    free(inode);

    return f;
}
