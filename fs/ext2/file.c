#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>

struct file *ext2_open(struct filesystem *, char *);

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
        free(fn);
        return 2;
    }

    /* this means that the tokenizing has at least one token, so
     * let us find the inode number of that token */
    int ino = 2;
    while (pch != 0) {
        //printk("looking for \"%s\" in inode %d: ", pch, ino);
        ino = ext2_read_directory(fs, ino, pch);
        if (ino < 0) {
            //printk("ENOENT\n");
            free(fn);
            return -ENOENT;
        }
        //printk("inode: %d\n");
        pch = strtok_r(0, "/", &lasts);
    }
    free(fn);
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

    struct ext2_inode *ibuf = malloc(EXT2_PRIV(fs)->inodesize);
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

    free(ibuf);

    return 0;
}

int ext2_file_fstat(struct file *f, struct stat *st)
{
    struct ext2_inode *ibuf = malloc(EXT2_PRIV(f->fs)->inodesize);
    //int ino = ext2_find_file_inode(f->fs, f->respath);
    int ino = EXT2_FILE_PRIV(f)->inode_no;

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

    free(ibuf);

    return 0;
}

int
ext2_file_read_block(struct file *f, struct ext2_inode *ibuf, void *buf, size_t b)
{
    struct filesystem *fs = f->fs;
    //struct ext2_inode *ibuf = malloc(EXT2_PRIV(fs)->inodesize);
    //int ino = ext2_find_file_inode(fs, f->respath);
    int ino = EXT2_FILE_PRIV(f)->inode_no;
    int bs = EXT2_PRIV(fs)->blocksize;
    uint32_t p = bs / sizeof(uint32_t);
    if (ino < 0) {
        free(ibuf);
        return ino;
    }

    if (!ibuf)
        return -ENOMEM;

    //ext2_read_inode(fs, ibuf, ino);

    if (b < 12) {
        ext2_read_block(fs, buf, ibuf->dbp[b]);
    } else {
        if (b < 12 + p) {
            uint32_t *bb = malloc(bs);
            if (!bb) {
                //free(ibuf);
                return -ENOMEM;
            }
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
        } else if (b < 12 + p + p * p + p) {
            int a = b - 12;
            int b = a - p;
            int c = b - p * p;
            int d = c / (p * p);
            int e = c - d * p * p;
            int f = e / p;
            int g = e - f * p;

            uint32_t *tmp = malloc(bs);
            ext2_read_block(fs, tmp, ibuf->triply_block);

            uint32_t nblock = ((uint32_t *)tmp)[d];
            ext2_read_block(fs, tmp, nblock);

            nblock = ((uint32_t *)tmp)[f];
            ext2_read_block(fs, tmp, nblock);

            unsigned int out = ((uint32_t  *)tmp)[g];
            free(tmp);

            ext2_read_block(fs, buf, out);
            goto exit;
        }
        //panic("%s over the maximum read block %d", __func__, b);
        return 0;
    }

exit:    //free(ibuf);
    return 0;
}

int
ext2_write_file_block(struct file *f, struct ext2_inode *inode, int b, void *buf)
{
    struct filesystem *fs = f->fs;
    int bs = EXT2_PRIV(fs)->blocksize;

    uint32_t p = bs / sizeof(uint32_t);

    if (b < 12) {
        ext2_write_block(fs, buf, inode->dbp[b]);
    } else {
        if (b < 12 + p) {
            uint32_t *bb = malloc(bs);
            if (!bb)
                return -ENOMEM;
            //printk("SLB: %d\n", inode->singly_block);
            ext2_read_block(fs, bb, inode->singly_block);
            //printk("ORIG %d\n", b);
            b = bb[b - 12];
            //printk("TRY %d\n", b);
            ext2_write_block(fs, buf, b);
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

            //printk("reading %d in the doubly block %d\n", b, inode->doubly_block);
            ext2_read_block(fs, buf1, inode->doubly_block);

            uint32_t nblock = buf1[C];
            //printk("reading in the refd block: %d from loc %d\n", nblock, C);
            ext2_read_block(fs, buf1, nblock);

            //printk("doubly: reading %d from loc %d\n", buf1[D], D);
            ext2_write_block(fs, buf, buf1[D]);

            free(buf1);
            goto exit;
        }
        panic("Triply block are not yet supported\n");
        return -ENOSYS;
    }

exit:
    return 0;
}

int
ext2_read_file(struct file *f, void *buf, size_t count)
{
    int rc;

    if (!count)
        return 0;

    if (!f || !buf)
        return -EINVAL;

    //int inode = ext2_find_file_inode(f->fs, f->respath);
    int inode = EXT2_FILE_PRIV(f)->inode_no;
    if (inode < 0)
        return inode;

    int total = 0;
    int bs = EXT2_PRIV(f->fs)->blocksize;

    /* determine if there is things left to read */
    struct ext2_inode *theinode = malloc(EXT2_PRIV(f->fs)->inodesize);
    if (!theinode)
        return -ENOMEM;

    ext2_read_inode(f->fs, theinode, inode);

    if (f->fpos >= theinode->size) {
        free(theinode);
        return 0;
    }

    /* determine how many blocks do we need */
    int blocks = count / bs;
    int off = count % bs;
    int rblocks = 0; /* blocks read so far */

    /* determine position */
    int cblock = f->fpos / bs;
    int coff = f->fpos % bs;

    int start_block = f->fpos / bs;
    uint32_t end = f->fpos + count;
    if (end > theinode->size)
        end = theinode->size;
    int end_block = end / bs;
    uint32_t end_size = end - end_block * bs;
    uint32_t to_read = end - f->fpos;

    uint8_t *buffer = malloc(bs);
    if (!buffer) {
        free(theinode);
        return -ENOMEM;
    }

	if (start_block == end_block) {
		ext2_file_read_block(f, theinode, buffer, start_block);
		memcpy(buf, (uint8_t *)(((uint32_t)buffer) + (f->fpos % bs)), to_read);
        total += to_read;
	} else {
		uint32_t block_offset;
		uint32_t blocks_read = 0;
		for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
			if (block_offset == start_block) {
				ext2_file_read_block(f, theinode, buffer, block_offset);
				memcpy(buf, (uint8_t *)(((uint32_t)buffer) + (f->fpos % bs)), bs - (f->fpos % bs));
                total += (bs - (f->fpos % bs));
			} else {
				ext2_file_read_block(f, theinode, buffer, block_offset);
				memcpy(buf + bs * blocks_read - (f->fpos % bs), buffer, bs);
                total += bs;
			}
		}
		if (end_size) {
			ext2_file_read_block(f, theinode, buffer, end_block);
			memcpy(buf + bs * blocks_read - (f->fpos % bs), buffer, end_size);
            total += end_size;
		}
	}

    f->fpos += total;

    rc = total;
exit:
    free(theinode);
    free(buffer);
    return rc;
}

size_t
ext2_write_file(struct file *f, void *buf, size_t count)
{
    int rc, i;

    if (!count) {
        printk("REJECT 1\n");
        return 0;
    }

    if (!f || !buf) {
        printk("REJECT 2\n");
        return -EINVAL;
    }

    //int ino = ext2_find_file_inode(f->fs, f->respath);
    int ino = EXT2_FILE_PRIV(f)->inode_no;
    if (ino < 0) {
        printk("REJECT 3 %s\n", f->respath);
        return ino;
    }

    struct ext2_inode *inode = malloc(EXT2_PRIV(f->fs)->inodesize);
    if (!inode) {
        printk("REJECT 4\n");
        return -ENOMEM;
    }

    if (ext2_read_inode(f->fs, inode, ino)) {
        free(inode);
        printk("REJECT 5\n");
        return -ENOMEM;
    }

    struct filesystem *fs = f->fs;

    int total = 0;
    int bs = EXT2_PRIV(f->fs)->blocksize;

    /* determine how many blocks do we need */
    int blocks = count / bs;
    int off = count % bs;
    int rblocks = 0; /* blocks read so far */

    /* determine position */
    int cblock = f->fpos / bs;
    int coff = f->fpos % bs;

    if (f->fpos > inode->size) {
        //free(inode);
        //printk("POOOP fpos %d size %d\n", f->fpos, inode->size);
        //return -EFBIG;
    }

    uint32_t end          = f->fpos + count;
    uint32_t start_block  = f->fpos / bs;
	uint32_t end_block    = end / bs;
	uint32_t end_size     = end - end_block * bs;
	uint32_t size_to_read = end - f->fpos;

    /*printk("end %d start_block %d end_block %d end_size %d s2read %d\n",
            end, start_block, end_block, end_size, size_to_read);*/

    uint8_t *buffer = malloc(bs);
    if (!buffer) {
        free(inode);
        return -ENOMEM;
    }

    /* do the write */
    /*printk("start_block %d end %d end_block %d pos %d\n",
            start_block, end, end_block, f->fpos);*/

    if (start_block == end_block) {
        //printk("CASE 1\n");
        int b = ext2_inode_read_or_create(fs, ino, inode, start_block, buffer);
        if (b < 0) {
            free(buffer);
            free(inode);
            return b;
        }
        memcpy(buffer + coff, buf, size_to_read);
        total += size_to_read;
        //printk("GOT BLOCK %d\n", b);
        ext2_write_block(fs, buffer, b);
    } else {
        //printk("CASE 2\n");
        uint32_t block_offset;
        uint32_t blocks_read = 0;
        for (block_offset = start_block; block_offset < end_block; block_offset ++, blocks_read ++) {
            if (block_offset == start_block) {
                int b = ext2_inode_read_or_create(fs, ino, inode, block_offset, buffer);
                if (b < 0) {
                    free(buffer);
                    free(inode);
                    return b;
                }
                memcpy(buffer + coff, buf, bs - coff);
                total += bs - coff;
                ext2_write_block(fs, buffer, b);
            } else {
                int b = ext2_inode_read_or_create(fs, ino, inode, block_offset, buffer);
                if (b < 0) {
                    free(buffer);
                    free(inode);
                    return b;
                }
                //printk("%s: blocks_read %d, coff %d\n", __func__, blocks_read, coff);
                memcpy(buffer, buf + bs * blocks_read - coff, bs);
                total += bs;
                ext2_write_block(fs, buffer, b);
            }
        }
        if (end_size) {
            int b = ext2_inode_read_or_create(fs, ino, inode, end_block, buffer);
            if (b < 0) {
                free(buffer);
                free(inode);
                return b;
            }
            memcpy(buffer, buf + bs * blocks_read - coff, end_size);
            total += end_size;
            ext2_write_block(fs, buffer, b);
        }
    }

done:
    ext2_read_inode(fs, inode, ino);
    if (end > inode->size) {
        inode->size = end;
        ext2_write_inode(fs, inode, ino);
        f->length = inode->size;
    }
    f->fpos += total;

    rc = total;
exit:
    free(buffer);
    free(inode);
    return rc;
}

void *
ext2_dup_priv(void *priv)
{
    if (!priv)
        return NULL;

    struct ext2_file_priv *ret = malloc(sizeof(*ret));
    if (!ret)
        return NULL;

    memcpy(ret, priv, sizeof(*ret));

    return ret;
}

struct file *
ext2_create_file(struct filesystem *fs, char *path)
{
    struct ext2_inode *inode = malloc(EXT2_PRIV(fs)->inodesize);

    int ino = ext2_new_inode(fs, inode);
    //printk("%s: new inode is %d\n", __func__, ino);
    char *_path = __path_get_path(path);

    //printk("%s: base path is %s\n", __func__, _path);
    int base = ext2_find_file_inode(fs, _path);
    if (base < 0) {
        printk("%s: base was not found \"%s\"\n", __func__, path);
        return ERR_PTR(base);
    }
    //printk("%s: it is at inode %d\n", __func__, base);
    __path_free(_path);

    //printk("%s: the dirent will be called %s\n", __func__, __path_get_name(path));
    struct ext2_dir *dirent = ext2_new_dirent(ino, __path_get_name(path)); 
    ext2_place_dirent(fs, base, dirent);
    //printk("%s: placed\n", __func__);

    return ext2_open(fs, path);
}

int
ext2_filldir(struct file *f, struct linux_dirent *dirent)
{
    struct ext2_dir *edir = dirent_get(f->fs, EXT2_FILE_PRIV(f)->inode_no, f->fpos);
    if (!edir)
        return -ENOSPC;

    memset(dirent, 0, sizeof(*dirent));

    dirent->d_ino = edir->inode;
    dirent->d_off = f->fpos;
    dirent->d_reclen = edir->size;
    memcpy(dirent->d_name, dirent_get_name(edir), edir->namelength);

    dirent_free(edir);

    f->fpos ++;

    return 0;
}

int
ext2_file_close(struct file *filp)
{
    //free(filp->full_path);
    free(filp->respath);
    free(filp->priv);
    free(filp);
    return 0;
}

int
ext2_truncate_file(struct file *f, int size)
{
    struct filesystem *fs = f->fs;
    struct ext2_inode *inode_buf;
    int inode_no;

    /* FIXME: support truncate large files */
    if (size)
        return -EROFS;

    inode_no = EXT2_FILE_PRIV(f)->inode_no;

    inode_buf = malloc(EXT2_PRIV(fs)->inodesize);
    if (!inode_buf)
        return -ENOMEM;

    ext2_read_inode(fs, inode_buf, inode_no);

    inode_buf->size = 0;
    inode_buf->disk_sectors = 0;

    /* FIXME: free the blocks */
    for (int i = 0; i < 12; i ++)
        inode_buf->dbp[i] = 0;

    inode_buf->singly_block = 0;
    inode_buf->doubly_block = 0;
    inode_buf->triply_block = 0;

    ext2_write_inode(fs, inode_buf, inode_no);

    f->length = 0;
    f->fpos = 0;

    free(inode_buf);

    return 0;
}

struct file_operations ext2_fops = {
    .read = ext2_read_file,
    .write = ext2_write_file,
    .truncate = ext2_truncate_file,
    .readdir = ext2_filldir,
    .fstat = ext2_file_fstat,
    .close = ext2_file_close,
};


struct file *ext2_open(struct filesystem *fs, char *p)
{
    struct file *f;
    struct ext2_inode *inode;
    struct ext2_file_priv *priv;
    int ino;

    if (!fs || !p)
        return (void *) -EINVAL;

    f = malloc(sizeof(*f));
    if (!f)
        return (void *) -ENOMEM;

    inode = malloc(EXT2_PRIV(fs)->inodesize);
    if (!inode) {
        free(f);
        return (void *) -ENOMEM;
    }

    priv = malloc(sizeof(*priv));
    if (!priv) {
        free(inode);
        free(f);
        return ERR_PTR(-ENOMEM);
    }

    ino = ext2_find_file_inode(fs, p);
    if (ino == -ENOENT) {
        free(priv);
        free(inode);
        free(f);
        //printk("NO FUCKING FILE %s FOUND BITCHEZ\n", p);
        return (void *) -ENOENT;
    }

    priv->inode_no = ino;

    ext2_read_inode(fs, inode, ino);

    f->fops = &ext2_fops;
    f->fs = fs;
    f->respath = strdup(p);
    f->isdir = ((inode->type & 0xf000) == INODE_TYPE_DIRECTORY);
    f->fpos = 0;
    f->length = inode->size;
    f->refc = 1;
    f->priv = priv;

    free(inode);

    return f;
}
