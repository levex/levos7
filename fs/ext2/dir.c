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

static inline int
__get_dirent_min_length(struct ext2_dir *dirent)
{
    return 9 + dirent->namelength;
}

static inline void
__minimize_dirent(struct ext2_dir *dirent)
{
    dirent->size = __get_dirent_min_length(dirent);
}

static inline void
__maximize_dirent(struct ext2_dir *dirent, int soff, int blocksize)
{
    printk("maximizing soff %d start: %d\n", soff, dirent->size);
    while ((soff + dirent->size) % blocksize)
        dirent->size ++;
    printk("maximizing end: %d\n", dirent->size);
}

static struct ext2_dir *
__ext2_find_largest_dirent_space(struct ext2_dir *dirbuf, int length, int bs)
{
    int off;
    struct ext2_dir *p = dirbuf;

    for (off = 0; off < bs; off += p->size) {
        p = ((void *)dirbuf) + off;

        int minlength = __get_dirent_min_length(p);

        /* can we minimize this dirent enough ? */
        if ((p->size - minlength) > length + 1)
            return p;
    }

fail:
    return (void *) -1;
}

/*
 * internal function to find a location in the inode to put the dirent
 */
int
__ext2_place_dirent(struct filesystem *fs, struct ext2_inode *inode,
                         int ino, struct ext2_dir *dirent)
{
    int i;
    struct ext2_priv_data *p = EXT2_PRIV(fs);

    struct ext2_dir *dirbuf = malloc(p->blocksize);
    if (!dirbuf)
        return -ENOMEM;

    /* loop through the DBPs */
    for (i = 0; i < 12; i ++) {
        /* Case 1.1, the DBP doesn't exist */
        if (inode->dbp[i] == 0) {
            void *buffer = malloc(p->blocksize);
            if (!buffer) {
                free(dirbuf);
                return -ENOMEM;
            }

            memcpy(buffer, dirent, __get_dirent_min_length(dirent));
            __minimize_dirent(buffer);
            
            /* add a new block with the dirent into the inode */
            ext2_inode_add_block(fs, ino, buffer);
            goto done;
        } else {
            /* Case 1.2, a DBP exists, find suitable place */

            ext2_read_block(fs, dirbuf, inode->dbp[i]);

            struct ext2_dir *before = __ext2_find_largest_dirent_space(dirbuf,
                            __get_dirent_min_length(dirent), p->blocksize);
            if ((int) before == -1)
                continue;

            printk("There is enough space after dirent \"%s\" (%d bytes)\n",
                    (void *) before + sizeof(before) + 1, before->size);

            /* there is enough space to put this dirent after "before" */

            if ((void *)before + before->size >= (void *)dirbuf + p->blocksize ) {
                struct ext2_dir *new;
                int dirent_minlength = __get_dirent_min_length(dirent);
                /* 
                 * in this case we would overflow, so we don't need to check
                 * the "after"
                 */
                __minimize_dirent(before);

                new = (void *)before + before->size;

                memcpy(new, dirent, dirent_minlength);
                /* maximize dirent */
                __maximize_dirent(new, (int) new - (int)dirbuf, p->blocksize);

                /* write back the block and we are done */
                ext2_write_block(fs, dirbuf, inode->dbp[i]);

                goto done;
            }

            /* get a pointer to "after" */
            struct ext2_dir *after = (void *)before + before->size;

            goto done;
        }
    }

    /* TODO: singly */
    /* TODO: doubly */
    /* TODO: triply */

done:;
    /* increase the links count of the inode */
    struct ext2_inode *tmpinode = malloc(sizeof(*tmpinode));
    ext2_read_inode(fs, tmpinode, dirent->inode);
    tmpinode->hardlinks ++;
    ext2_write_inode(fs, tmpinode, dirent->inode);
    free(dirbuf);
    return 0;
}

int
ext2_place_dirent(struct filesystem *fs, int ino, struct ext2_dir *dirent)
{
    int ret;
    struct ext2_inode *buf = malloc(sizeof(*buf));

    if (!buf)
        return -ENOMEM;

    ext2_read_inode(fs, buf, ino);

    ret = __ext2_place_dirent(fs, buf, ino, dirent);

    free(buf);
    return ret;
}

/*
 * Creates a new ext2_dirent that points to inode @inode
 * and has the name @name
 */
struct ext2_dir *
ext2_new_dirent(int inode, char *name)
{
    int rec_len = 8 + strlen(name) + 1;
    struct ext2_dir *buf;

    while (rec_len % 4)
        rec_len ++;

    buf = malloc(rec_len);
    if (!buf)
        return (void *) -ENOMEM;

    memset(buf, 0, rec_len);

    buf->inode = inode;
    buf->size = rec_len;
    buf->namelength = strlen(name);
    /* XXX: reserved should denote filetype */
    memcpy(((void *) &buf->reserved) + sizeof(buf->reserved), name, strlen(name));

    return buf;
}
