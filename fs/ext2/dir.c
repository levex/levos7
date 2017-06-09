#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>

int ext2_read_directory(struct filesystem *fs, int dino, char *f)
{
    /* read the directory inode in */
    struct ext2_inode *inode = malloc(EXT2_PRIV(fs)->inodesize);
    ext2_read_inode(fs, inode, dino);

    //printk("type: 0x%x\n", inode->type);
    if (inode->type & 0x4000 == 0) {
        free(inode);
        return -ENOTDIR;
    }

    //printk("size: %d\n", inode->size);

    /* the block pointers contain some 'struct ext2_dir's, so parse */
    void *bbuf = malloc(EXT2_PRIV(fs)->blocksize);

    for (int i = 0; i < 12; i++) {
        ext2_read_block(fs, bbuf, inode->dbp[i]);
        //printk("read block %d = %d\n", i, inode->dbp[i]);
        struct ext2_dir *d = (void *)bbuf;
        if (d->size == 0 || d->namelength == 0)
            break;
        int r = 0;
        while (r < EXT2_PRIV(fs)->blocksize) {
            //printk("compare: %d %d %s %s\n",
                    //strlen(f), d->namelength, &d->reserved + 1, f);
            if (strlen(f) == d->namelength &&
                    strncmp(&d->reserved + 1, f, d->namelength) == 0) {
                int k = d->inode;
                free(inode);
                free(bbuf);
                //printk("LOL k %d\n", k);
                return k == 0 ? -ENOENT : k;
            }
            r += d->size;
            if (d->size == 0 || d->namelength == 0) {
                goto c1;
            }
            d = (struct ext2_dir *)((uintptr_t)d + d->size);
        }
        c1:;
    }
    //printk("LEEEL\n");

    free(bbuf);
    free(inode);
    return -ENOENT;
}

struct ext2_dir *
dirent_get(struct filesystem *fs, int inode, int n)
{
    int i = 0, b;

    struct ext2_inode *ibuf = malloc(EXT2_PRIV(fs)->inodesize);
    if (!ibuf)
        return NULL;

    char *buffer = malloc(EXT2_PRIV(fs)->blocksize);
    if (!buffer) {
        free(ibuf);
        return NULL;
    }

    ext2_read_inode(fs, ibuf, inode);

    /* loop thru the DBPs */
    for (b = 0; b < 12; b ++) {
        if (ibuf->dbp[b]) {
            ext2_read_block(fs, buffer, ibuf->dbp[b]);
            struct ext2_dir *curr = (struct ext2_dir *) buffer;
            //printk("B == %d\n", b);
next:
            if (i == n) {
                if (curr->inode == 0) {
                    if (curr->size == 0) {
                        printk("uh this is weird\n");
                        continue;
                    }

                    curr = ((void *)curr) + curr->size;
                    if ((void *) curr >= (void *)buffer + EXT2_PRIV(fs)->blocksize)
                        continue;
                    //i ++;
                    //printk("%s: 2\n", __func__);
                    goto next;
                }

                struct ext2_dir *ret = malloc(curr->size);
                if (!ret) {
                    free(ibuf);
                    free(buffer);
                    return NULL;
                }

                memcpy(ret, curr, curr->size);
                free(ibuf);
                free(buffer);
                //printk("%s: returning one with \"%s\"\n",
                        //__func__, &ret->reserved);
                return ret;
            } else {
                //printk("i %d n %d curr->size: %d \"%s\" ino:%d\n", i, n, curr->size,
                        //dirent_get_name(curr), curr->inode);
                panic_ifnot(curr->size != 0);
                curr = ((void *)curr) + curr->size;
                if ((void *) curr >= (void *)buffer + EXT2_PRIV(fs)->blocksize)
                    continue;
                if (curr->inode != 0)
                    i ++;
                //printk("%s: 1\n", __func__);
                goto next;
            }
        } else {
            //printk("%s: ran out of DBPs\n", __func__);
            free(ibuf);
            free(buffer);
            return NULL;
        }
    }

    free(ibuf);
    free(buffer);

    panic("unable to parse doubly for directory\n");
    return NULL;
}

void
dirent_free(void *buf)
{
    free(buf);
}

static inline int
__get_dirent_min_length(struct ext2_dir *dirent)
{
    return sizeof(*dirent) + dirent->namelength;
}

char *
dirent_get_name(struct ext2_dir *before)
{
    return (void *) before + sizeof(*before);
}

static inline void
__minimize_dirent(struct ext2_dir *dirent)
{
    //printk("minimized dirent %s from %d", dirent_get_name(dirent));
    dirent->size = __get_dirent_min_length(dirent);
    while (dirent->size % 4)
            dirent->size ++;
    //printk(" to %d\n", dirent->size);
}

static inline void
__maximize_dirent(struct ext2_dir *dirent, int soff, int blocksize)
{
    //printk("maximizing soff %d start: %d\n", soff, dirent->size);
    while ((soff + dirent->size) % blocksize)
        dirent->size ++;
    //printk("maximizing end: %d\n", dirent->size);
}

static struct ext2_dir *
__ext2_find_largest_dirent_space(struct ext2_dir *dirbuf, int length, int bs)
{
    int off;
    struct ext2_dir *p = dirbuf;

    for (off = 0; off < bs; off += p->size) {
        p = ((void *)dirbuf) + off;
        if (p->size == 0) {
            printk("off: %d name: \"%s\"\n", off, dirent_get_name(p));
            panic_ifnot(p->size != 0);
        }
        //printk("dirent space: \"%s\" off %d size %d\n",
                //dirent_get_name(p), off, p->size);

        if (off + p->size >= bs)
            return p;

#if 0

        int minlength = __get_dirent_min_length(p);

        /* can we minimize this dirent enough ? */
        if ((p->size - minlength) > length + 1)
            return p;
#endif
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
    int i, old_secs;
    struct ext2_priv_data *p = EXT2_PRIV(fs);

    struct ext2_dir *dirbuf = malloc(p->blocksize);
    if (!dirbuf)
        return -ENOMEM;

    old_secs = inode->disk_sectors;

    /* loop through the DBPs */
    for (i = 0; i < 12; i ++) {
        /* Case 1.1, the DBP doesn't exist */
        if (inode->dbp[i] == 0) {
            /* in reality, this case can't happen because it must contain
             * at least a "." and a "..", however in order to reuse
             * this in mkdir, we handle this case
             */
            void *buffer = malloc(p->blocksize);
            if (!buffer) {
                free(dirbuf);
                return -ENOMEM;
            }

            memset(buffer, 0, p->blocksize);

            dirent->size = p->blocksize;
            memcpy(buffer, dirent, __get_dirent_min_length(dirent));
            
            /* add a new block with the dirent into the inode */
            int blockovich = ext2_inode_add_block(fs, -1, ino, inode);
            ext2_write_block(fs, buffer, blockovich);

            //printk("added block dirent: \"%s\"\n", dirent_get_name(dirent));

            free(buffer);
            goto done;
        } else {
            /* Case 1.2, a DBP exists, find suitable place */

            ext2_read_block(fs, dirbuf, inode->dbp[i]);

            struct ext2_dir *before = __ext2_find_largest_dirent_space(dirbuf,
                            __get_dirent_min_length(dirent), p->blocksize);
            if ((int) before == -1)
                continue;

            //printk("There is enough space after dirent \"%s\" (%d bytes)\n",
                    //(void *) before + sizeof(before) + 1, before->size);

            /* there is enough space to put this dirent after "before" */

            if ((void *)before + before->size >= (void *)dirbuf + p->blocksize ) {
                struct ext2_dir *new;
                int dirent_minlength = __get_dirent_min_length(dirent);
                /* 
                 * in this case we would overflow, so we don't need to check
                 * the "after"
                 */
                //printk("before is at offset %d\n", (int) before - (int)dirbuf);
                __minimize_dirent(before);

                new = (void *)before + before->size;
                //printk("new is at offset %d\n", (int) new - (int)dirbuf);

                memcpy(new, dirent, dirent_minlength);

                /* maximize dirent */
                __maximize_dirent(new, (int) new - (int)dirbuf, p->blocksize);

                /* write back the block and we are done */
                ext2_write_block(fs, dirbuf, inode->dbp[i]);

                goto done;
            }
            printk("WARNING: could have placed the dirent between two entries\n");
            printk("This is unimplemented, so proceeding\n");
            continue;

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
    struct ext2_inode *tmpinode = malloc(EXT2_PRIV(fs)->inodesize);
    ext2_read_inode(fs, tmpinode, dirent->inode);
    tmpinode->hardlinks ++;

    if (tmpinode->disk_sectors != old_secs)
        tmpinode->size += p->blocksize;

    ext2_write_inode(fs, tmpinode, dirent->inode);
    free(tmpinode);
    free(dirbuf);
    return 0;
}

int
ext2_place_dirent(struct filesystem *fs, int ino, struct ext2_dir *dirent)
{
    int ret;
    struct ext2_inode *buf = malloc(EXT2_PRIV(fs)->inodesize);

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

int
ext2_mkdir(struct filesystem *fs, char *path, int mode)
{
    struct ext2_priv_data *priv = EXT2_PRIV(fs);
    int this_inode_no, parent_inode_no, rc, len = strlen(path);
    char *path_buf;
    struct ext2_inode *inode_buffer;

    path_buf = malloc(len + 4);
    if (!path_buf)
        return -ENOMEM;

    memset(path_buf, 0, len + 4);

    memcpy(path_buf, path, len);

    path_buf[len + 0] = '/';
    path_buf[len + 1] = '.';
    path_buf[len + 2] = '.';
    path_buf[len + 3] = 0;

    /* XXX: should check if all path elements are directories */

    char *parent = __canonicalize_path("/", path_buf);
    //printk("parent of \"%s\" is: \"%s\"\n", path, parent);

    /* check if the parent exists */
    parent_inode_no = ext2_find_file_inode(fs, parent);
    if (parent_inode_no <= 0) {
        free(path_buf);
        free(parent);
        //printk("inode_no ENOENT: %d\n", inode_no);
        rc = -ENOENT;
        goto ret;
    }

    /* check if it exists */
    this_inode_no = ext2_find_file_inode(fs, path);
    if (this_inode_no > 0) {
        free(path_buf);
        free(parent);
        //printk("inode_no EEXIST: %d\n", inode_no);
        rc = -EEXIST;
        goto ret;
    }

    /* allocate an inode */
    this_inode_no = ext2_alloc_inode(fs);
    if (this_inode_no < 0) {
        rc = this_inode_no;
        goto ret;
    }
    
    inode_buffer = malloc(EXT2_PRIV(fs)->inodesize);
    if (!inode_buffer) {
        /* FIXME: free inode */
        rc = -ENOMEM;
        goto ret;
    }

    //ext2_read_inode(fs, inode_buffer, this_inode_no);
    inode_buffer->type = S_IFDIR | /* make it RWXRWXRWX */ 0x0fff;
    inode_buffer->uid = 0;
    inode_buffer->size = 0;
    inode_buffer->last_access = 0;
    inode_buffer->last_modif = 0;
    inode_buffer->create_time = 0;
    inode_buffer->delete_time = 0;
    inode_buffer->gid = 0;
    inode_buffer->hardlinks = 0;
    inode_buffer->disk_sectors = 0; /* TODO This needs to be updated */
    inode_buffer->flags = 0;
    inode_buffer->ossv1 = 0;
    memset(&inode_buffer->dbp, 0, sizeof(inode_buffer->dbp));
    inode_buffer->singly_block = 0;
    inode_buffer->doubly_block = 0;
    inode_buffer->triply_block = 0;
    inode_buffer->gen_no = 0;
    inode_buffer->reserved1 = 0;
    inode_buffer->reserved2 = 0;
    inode_buffer->fragment_block = 0;
    memset(&inode_buffer->ossv2, 0, sizeof(inode_buffer->ossv2));

    ext2_write_inode(fs, inode_buffer, this_inode_no);

#if 0
    /* allocate a block to the inode */
    //int this_block = ext2_alloc_block(fs);
    rc = allocate_inode_block(fs, inode_buffer, this_inode_no, 0);
    if (rc < 0) {
        /* FIXME: free block */
        /* FIXME: free inode */
        goto ret2;
    }
#endif


    //printk("POOOP\n");
    /* write the . & .. entries */
    struct ext2_dir *dot_e = ext2_new_dirent(this_inode_no, ".");
    if (IS_ERR(dot_e)) {
        rc = -ENOMEM;
        goto ret2;
    }
    //printk("POOOP2\n");

    struct ext2_dir *dotdot_e = ext2_new_dirent(parent_inode_no, "..");
    if (IS_ERR(dotdot_e)) {
        rc = -ENOMEM;
        goto ret3;
    }
    //printk("POOOP3\n");

    rc = ext2_place_dirent(fs, this_inode_no, dot_e);
    if (rc)
        goto ret4;
    //printk("POOOP4\n");

    rc = ext2_place_dirent(fs, this_inode_no, dotdot_e);
    if (rc)
        goto ret4;
    //printk("POOOP5\n");

    /* FIXME: ideally undo a lot of our work here */

    /* add an entry to our parent */
    //printk("mkdir: creating the dir with name: [%s]\n", __path_get_name(path));
    struct ext2_dir *parentry = ext2_new_dirent(this_inode_no, __path_get_name(path));
    if (IS_ERR(parentry)) {
        rc = -ENOMEM;
        goto ret4;
    }
    //printk("POOOP6\n");

    rc = ext2_place_dirent(fs, parent_inode_no, parentry);
    if (rc)
        goto ret5;
    //printk("POOOP7\n");
    
    /* update the block group descriptor */
    struct ext2_block_group_desc *bgd = malloc(priv->blocksize);
    if (!bgd) {
        rc = -ENOMEM;
        goto ret5;
    }
    void *old_bgd = bgd;
    ext2_read_block(fs, bgd, priv->first_bgd);
    for(int i = 0; i < (this_inode_no - 1) / priv->sb.inodes_in_blockgroup; i++)
        bgd ++;

    bgd->num_of_dirs ++;
    ext2_write_block(fs, old_bgd, priv->first_bgd);

    /* done! */
    rc = 0;
ret5:
    free(parentry);
ret4:
    free(dotdot_e);
ret3:
    free(dot_e);
ret2:
    free(inode_buffer);
ret:
    free(path_buf);
    free(parent);
    return rc;
}
