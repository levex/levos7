#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>
#include <levos/bitmap.h>

int ext2_read_inode(struct filesystem *fs, struct ext2_inode *buf, int inode)
{
    struct ext2_priv_data *p = EXT2_PRIV(fs);
    uint32_t bg = (inode - 1) / p->sb.inodes_in_blockgroup;
    uint32_t i = 0;

    char *block_buf = malloc(p->blocksize);
    if (!block_buf)
        return -ENOMEM;
    ext2_read_block(fs, block_buf, p->first_bgd);

    struct ext2_block_group_desc *bgd = (void *)block_buf;
    for(i = 0; i < bg; i++)
        bgd ++;

    uint32_t index = (inode - 1) % p->sb.inodes_in_blockgroup;
    uint32_t block = (index * sizeof(struct ext2_inode)) / p->blocksize;
    ext2_read_block(fs, block_buf, bgd->block_of_inode_table + block);
    struct ext2_inode *_inode = (void *)block_buf;
    index = index % p->inodes_per_block;
    for (i = 0; i < index; i++)
        _inode ++;

    memcpy(buf, (void *) _inode, sizeof(struct ext2_inode));

    free(block_buf);
    return 0;
}


int
ext2_alloc_inode(struct filesystem *fs)
{
    struct ext2_priv_data *p = EXT2_PRIV(fs);
    int i;

    char *block_buf = malloc(p->blocksize);
    if (!block_buf)
        return -ENOMEM;

    /* read in the BGDT */
    ext2_read_block(fs, block_buf, p->first_bgd);

    /* loop through the BGs */
    struct ext2_block_group_desc *bgd = (void *)block_buf;
    for(i = 0; i < p->number_of_bgs ; i++, bgd ++) {
        if (bgd->num_of_unalloc_inode > 0) {
            struct bitmap bm;
            char *buffer;
            size_t found_inode;

            /* there is at least one inode here */
            buffer = malloc(p->blocksize);
            if (!buffer) {
                free(block_buf);
                return -ENOMEM;
            }

            /* read in the inode usage bitmap */
            ext2_read_block(fs, buffer, bgd->block_of_inode_usage_bitmap);

            /* open the bitmap */
            bitmap_create_using_buffer(p->sb.inodes_in_blockgroup,
                    buffer, &bm);

            /* find a free bit */
            found_inode = bitmap_scan_and_flip(&bm, 0, 1, 0);
            if (found_inode == BITMAP_ERROR) {
                printk("[ext2] CRITICAL: inconsistent inode bitmap\n");
                free(buffer);
                /* this is weird, try with the next bgd */
                continue;
            }
            /* we found a block, commit the bitmap */
            ext2_write_block(fs, buffer, bgd->block_of_inode_usage_bitmap);
            
            /* update the superblock */
            p->sb.unallocatedinodes --;
            ext2_write_superblock(fs);

            /* now update the BGD */
            bgd->num_of_unalloc_inode --;
            ext2_write_block(fs, block_buf, p->first_bgd);

            /* clean up */
            free(buffer);
            free(block_buf);

            /* return the block id */
            return i * p->sb.inodes_in_blockgroup + found_inode + 1;
        }
    }
    free(block_buf);
    return -1;
}

/* Creates a new inode in @inode, then allocates an inode number in @fs */
int
ext2_new_inode(struct filesystem *fs, struct ext2_inode *inode)
{
    int ino;
    struct ext2_priv_data *p = EXT2_PRIV(fs);

    /* first, allocate an inode number, ENOSPC if fails */
    ino = ext2_alloc_inode(fs);
    if (ino < 0)
        return -ENOSPC; /* we coerce into ENOSPC */

    /* clear out the buffer */
    memset(inode, 0, sizeof(*inode));

    /* by default, we set it to a file */
    /* TODO make this nice */
    inode->type = 0x8000 /* IFREG */| /* make it RWXRWXRWX */ 0x0fff;
    inode->uid = 0;
    inode->size = 0;
    inode->last_access = 0;
    inode->last_modif = 0;
    inode->create_time = 0;
    inode->delete_time = 0;
    inode->gid = 0;
    /* this inode is not in use yet but allocated, so set hardlinks to zero */
    inode->hardlinks = 0;
    /* we are not using any disk sector yet */
    inode->disk_sectors = 0; /* TODO This needs to be updated */
    /* we dont support any flags yet */
    inode->flags = 0;
    inode->ossv1 = 0;
    /* we dont have any blocks yet */
    memset(&inode->dbp, 0, sizeof(inode->dbp));
    //printk("sizeof(inode->dbp): %d\n", sizeof(inode->dbp));
    inode->singly_block = 0;
    inode->doubly_block = 0;
    inode->triply_block = 0;
    inode->gen_no = 0;
    inode->reserved1 = 0;
    inode->reserved2 = 0;
    inode->fragment_block = 0;
    memset(&inode->ossv2, 0, sizeof(inode->ossv2));
    //printk("sizeof(inode->ossv2): %d\n", sizeof(inode->ossv2));

    /* write the new inode */
    ext2_write_inode(fs, inode, ino);

    //printk("writing inode %d\n", ino);

    /* return the inode number */
    return ino;
}

int ext2_write_inode(struct filesystem *fs, struct ext2_inode *buf, int inode)
{
    struct ext2_priv_data *p = EXT2_PRIV(fs);
    uint32_t bg = (inode - 1) / p->sb.inodes_in_blockgroup;
    uint32_t i = 0;

    char *block_buf = malloc(p->blocksize);
    if (!block_buf)
        return -ENOMEM;

    /* read in the BGDs */
    ext2_read_block(fs, block_buf, p->first_bgd);

    /* read the BGD */
    struct ext2_block_group_desc *bgd = (void *)block_buf;
    for(i = 0; i < bg; i++)
        bgd ++;

    /* find the block and its index in it of the inode */
    uint32_t index = (inode - 1) % p->sb.inodes_in_blockgroup;
    uint32_t block = (index * sizeof(struct ext2_inode)) / p->blocksize;
    int final = bgd->block_of_inode_table + block;

    /* read that block and generate a pointer */
    ext2_read_block(fs, block_buf, final);
    struct ext2_inode *_inode = (void *)block_buf;
    index = index % p->inodes_per_block;
    for (i = 0; i < index; i++)
        _inode ++;

    /* copy our modified inode */
    memcpy(_inode, (void *) buf, sizeof(struct ext2_inode));

    /* write back the block */
    ext2_write_block(fs, block_buf, final);

    free(block_buf);
    return 0;
}

int
ext2_inode_get_block(struct filesystem *fs, struct ext2_inode *ibuf, int b)
{
    int bs = EXT2_PRIV(fs)->blocksize;
    uint32_t p = bs / sizeof(uint32_t);

    if (b < 12) {
        return ibuf->dbp[b];
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
            free(bb);
            return b;
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
            if (buf1[C] == 0)
                panic("this case is unhandled\n");
            //printk("reading in the refd block: %d from loc %d\n", nblock, C);
            ext2_read_block(fs, buf1, nblock);

            //printk("doubly: reading %d from loc %d\n", buf1[D], D);

            free(buf1);
            return buf1[D];
        }
        printk("Triply block are not yet supported\n");
        return -ENOSYS;
    }
    panic("%s: this can't happen\n", __func__);
    __not_reached();
    return -1;
}

int
ext2_inode_read_or_create(struct filesystem *fs, int ino, struct ext2_inode *inode, int b, void *buf)
{
    ext2_read_inode(fs, inode, ino);

    int the_block = ext2_inode_get_block(fs, inode, b);
    if (the_block == 0) {
        int ret = ext2_inode_add_block(fs, ino, inode);
        memset(buf, 0, EXT2_PRIV(fs)->blocksize);
        //printk("%s: didn't exist, allocated %d (%s)\n", __func__, ret, errno_to_string(ret));
        return ret;
    }

    //printk("%s: existed: return %d\n", __func__, the_block);
    ext2_read_block(fs, buf, the_block);
    return the_block;
}

int
set_block_number(struct filesystem *fs,
                 struct ext2_inode *inode,
                 int inode_no,
                 int iblock,
                 int rblock)
{
    int bs = EXT2_PRIV(fs)->blocksize;
	unsigned int p = bs / sizeof(uint32_t);

	unsigned int a, b, c, d, e, f, g;

	uint8_t *tmp;

	if (iblock < 12) {
		inode->dbp[iblock] = rblock;
        ext2_write_inode(fs, inode, inode_no);
		return 0;
	} else if (iblock < 12 + p) {
		if (!inode->singly_block) {
			unsigned int block_no = ext2_alloc_block(fs);
			if (!block_no)
                return -ENOSPC;
			inode->singly_block = block_no;
			ext2_write_inode(fs, inode, inode_no);
		}
		tmp = malloc(bs);
        if (!tmp)
            panic("OOM\n");

		ext2_read_block(fs, tmp, inode->singly_block);

		((uint32_t *)tmp)[iblock - 12] = rblock;
		ext2_write_block(fs, tmp, inode->singly_block);

		free(tmp);
		return 0;
	} else if (iblock < 12 + p + p * p) {
		a = iblock - 12 ;
		b = a - p;
		c = b / p;
		d = b - c * p;

		if (!inode->doubly_block) {
			unsigned int block_no = ext2_alloc_block(fs);
			if (!block_no)
                return -ENOSPC;
			inode->doubly_block = block_no;
			ext2_write_inode(fs, inode, inode_no);
		}

		tmp = malloc(bs);
        if (!tmp)
            panic("OOM2\n");
		ext2_read_block(fs, tmp, inode->doubly_block);

		if (!((uint32_t *)tmp)[c]) {
			unsigned int block_no = ext2_alloc_block(fs);
			if (!block_no)
                goto no_space_free;
			((uint32_t *)tmp)[c] = block_no;
			ext2_write_block(fs, tmp, inode->doubly_block);
		}

		uint32_t nblock = ((uint32_t *)tmp)[c];
		ext2_read_block(fs, tmp, nblock);

		((uint32_t  *)tmp)[d] = rblock;
		ext2_write_block(fs, tmp, nblock);

		free(tmp);
		return 0;
	} else {
        panic("TRIPLY BLOCKS NOT SUPPORTED IN %s\n", __func__);
    }

    return -ENOSYS;
no_space_free:
	free(tmp);
	return -ENOSPC;
}

int
allocate_inode_block(struct filesystem *fs,
                     struct ext2_inode *inode,
                     int inode_no,
                     int block)
{
    int bs = EXT2_PRIV(fs)->blocksize;
	unsigned int block_no = ext2_alloc_block(fs);

	if (!block_no || block_no < 0) {
        //printk("OUCH THIS IS BAD block_no %d\n", block_no);
        return -ENOSPC;
    }

	set_block_number(fs, inode, inode_no, block, block_no);

	unsigned int t = (block + 1) * (bs / 512);
	if (inode->disk_sectors < t) {
		inode->disk_sectors = t;
	}
	ext2_write_inode(fs, inode, inode_no);

	return block_no;
}

int
ext2_inode_add_block(struct filesystem *fs, int ino, struct ext2_inode *inode)
{
    int rblock = inode->disk_sectors / (EXT2_PRIV(fs)->blocksize / 512);
    //printk("RBLOCK BECAME %d\n", rblock);
    return allocate_inode_block(fs, inode, ino, rblock);
}


/* allocates a new block and then appends it to the inode, copying the data
 * buf
 */
int
old_ext2_inode_add_block(struct filesystem *fs, int ino, void *buf)
{
    int block_no, i, j, k, extra_no = 0, extra_no2 = 0;
    struct ext2_inode inode;
    struct ext2_priv_data *p = EXT2_PRIV(fs);
    uint32_t *block_buf;

    //printk("trying to add a block to inode %d\n", ino);
    
    /* allocate the block */
    block_no = ext2_alloc_block(fs);

    /* if we failed to allocate a block, we coerce to ENOSPC */
    if (block_no < 0) {
        //printk("block_no %d\n", block_no);
        return -ENOSPC;
    }

    /* read in the inode */
    ext2_read_inode(fs, &inode, ino);

    /* we have allocated a block, let's try to grow the inode,
     * first find a free block pointer
     */

    /* Case 1, we find a free DPB */
    for (i = 0; i < 12; i ++) {
        if (inode.dbp[i] == 0) {
            /* lucky, we can append the block here */
            inode.dbp[i] = block_no;
            /* write back the inode */
            ext2_write_inode(fs, &inode, ino);
            //printk("%s: wrote to new block %d DBP %d\n", __func__, block_no, i);
            goto done;
        }
    }

    /* Case 2, try the singly linked block */
    block_buf = malloc(p->blocksize);
    if (!block_buf) {
        /* FIXME: free block_no */
        return -ENOMEM;
    }

    /* read in the singly block */
    ext2_read_block(fs, block_buf, inode.singly_block);

    /* loop through the singly block */
    for (i = 0; i < p->blocksize / sizeof(uint32_t); i ++) {
        if (block_buf[i] == 0) {
            /* found a spot! */
            block_buf[i] = block_no;
            /* write back the singly */
            ext2_write_block(fs, block_buf, inode.singly_block);
            goto done;
        }
    }

    /* Case 3, we need to go through the doubly linked */

    /* Case 3.1, the doubly pointer does not exist */
    if (inode.doubly_block == 0) {
        /* allocate a new block */
        extra_no = ext2_alloc_block(fs);

        if (extra_no < 0) {
            /* TODO: free block_no */
            free(block_buf);
            return -ENOSPC;
        }

        /* set the doubly in the inode */
        inode.doubly_block = extra_no;

        /* write the inode to the disk */
        ext2_write_inode(fs, &inode, ino);
    }

    ext2_read_block(fs, block_buf, inode.doubly_block);

    /* Case 3.2, the doubly pointer exists */
    for (i = 0; i < p->blocksize / sizeof(uint32_t); i ++) {
        if (block_buf[i] == 0) {
            /* Case 3.2.1, we need to allocate a block to store the singly */
            int singly_block = ext2_alloc_block(fs);
            if (singly_block < 0) {
                /* TODO: free block_no and extra_no*/
                free(block_buf);
                return -ENOSPC;
            }
            /* Write back the new pointer */
            block_buf[i] = singly_block;

            ext2_write_block(fs, block_buf, inode.doubly_block);
            /* Since we allocated this block, we can just write to the very
             * first direct pointer
             */
            memset(block_buf, 0, p->blocksize);
            block_buf[0] = block_no;
            /* write the singly block back */
            ext2_write_block(fs, block_buf, singly_block);
            free(block_buf);
            /* we are done */
            goto done;
        } else {
            /* Case 3.2.2, we found a singly block in a doubly ptr, so
             * try that block
             */
            uint32_t *singly = malloc(p->blocksize);
            if (!singly) {
                /* TODO: free block_no and extra_no */
                free(block_buf);
                return -ENOMEM;
            }
            ext2_read_block(fs, singly, block_buf[i]);
            /* loop through the pointers */
            for (j = 0; j < p->blocksize / sizeof(uint32_t); j ++) {
                if (singly[j] == 0) {
                    /* found a spot! */
                    singly[j] = block_no;
                    /* write back this singly */
                    ext2_write_block(fs, singly, block_buf[i]);
                    free(singly);
                    free(block_buf);
                    /* we are done */
                    goto done;
                }
            }
        }

        /* Case 4, in desperation, we look at the triply linked ptrs */
        /* Case 4.1, the triply block does not exist */
        if (inode.triply_block == 0) {
            extra_no = ext2_alloc_block(fs);
            if (extra_no < 0) {
                /* TODO: free block_no and extra_no */
                free(block_buf);
                return -ENOSPC;
            }

            inode.triply_block = extra_no;

            /* write the inode to the disk */
            ext2_write_inode(fs, &inode, ino);
        }
        
        /* read in the triply block */
        ext2_read_block(fs, block_buf, inode.triply_block);

        /* Case 4.2, the triply block exists */
        for (i = 0; i < p->blocksize / sizeof(uint32_t); i ++) {
            if (block_buf[i] == 0) {
                /* Case 4.2.1, the doubly block does not exist */
                uint32_t *extra_buf;

                /* allocate the doubly block */
                extra_no = ext2_alloc_block(fs);
                if (extra_no < 0) {
                    /* TODO: free block_no and extra_no */
                    free(block_buf);
                    return -ENOSPC;
                }

                /* set the doubly block */
                block_buf[i] = extra_no;

                /* extra_buf will serve as the doubly block */
                extra_buf = malloc(p->blocksize);
                if (!extra_buf) {
                    /* TODO: free block_no and extra_no */
                    free(block_buf);
                    return -ENOMEM;
                }

                /* set it to all zeroes */
                memset(extra_buf, 0, p->blocksize);

                /* allocate the singly block */
                extra_no2 = ext2_alloc_block(fs);
                if (extra_no2 < 0) {
                    free(extra_buf);
                    free(block_buf);
                    return -ENOSPC;
                }

                /* set the singly block in the doubly */
                extra_buf[0] = extra_no2;

                /* write the doubly block */
                ext2_write_block(fs, extra_buf, extra_no);

                /* reuse extra_buf as the singly block */
                memset(extra_buf, 0, p->blocksize);
                extra_buf[0] = block_no;

                /* write the singly block */
                ext2_write_block(fs, extra_buf, extra_no2);
                free(extra_buf);

                /* all done */
                goto done;
            } else {
                /* Case 4.2.2, the doubly block exists */

                /* read in the doubly block */
                uint32_t *doubly = malloc(p->blocksize);
                if (!doubly) {
                    /* TODO: free block_no and extra_no */
                    free(block_buf);
                    return -ENOMEM;
                }
                ext2_read_block(fs, doubly, block_buf[i]);

                /* loop through the doubly */
                for (j = 0; j < p->blocksize / sizeof(uint32_t); j ++) {
                    if (doubly[j] == 0) {
                        /* Case 4.2.2.1, the singly block does not exist */
                        
                        /* allocate a block to be the singly block */
                        extra_no = ext2_alloc_block(fs);
                        if (extra_no < 0) {
                            /* TODO: free block_no and extra_no */
                            free(doubly);
                            free(block_buf);
                            return -ENOSPC;
                        }

                        /* allocate a buffer for the singly block */
                        uint32_t *singly = malloc(p->blocksize);
                        if (!singly) {
                            /* TODO: free block_no and extra_no */
                            free(doubly);
                            free(block_buf);
                            return -ENOMEM;
                        }

                        /* make it all zeroes */
                        memset(singly, 0, p->blocksize);

                        /* put in our block */
                        singly[0] = block_no;

                        /* write the singly block */
                        ext2_write_block(fs, singly, extra_no);

                        /* set it in the doubly */
                        doubly[j] = extra_no;

                        /* write back the doubly */
                        ext2_write_block(fs, doubly, block_buf[i]);

                        /* we are done */
                        free(doubly);
                        free(singly);
                        goto done;
                    } else {
                        /* Case 4.2.2.2, the singly block exists */
                        uint32_t *singly = malloc(p->blocksize);
                        if (!singly) {
                            /* TODO: free block_no and extra_no */
                            free(doubly);
                            free(block_buf);
                            return -ENOMEM;
                        }

                        /* read in the singly */
                        ext2_read_block(fs, singly, doubly[j]);

                        /* loop thru the singly */
                        for (k = 0; k < p->blocksize / sizeof(uint32_t); k ++) {
                            if (singly[k] == 0) {
                                /* place the block */
                                singly[k] = block_no;
                                /* write back the singly block */
                                ext2_write_block(fs, singly, doubly[j]);
                                /* we are done */
                                free(doubly);
                                free(singly);
                                goto done;
                            }
                        }
                    }
                }
            }
        }
    }

done:
    /* write the block to the disk */
    if (buf)
        ext2_write_block(fs, buf, block_no);

    /* success */
    return block_no;
}
