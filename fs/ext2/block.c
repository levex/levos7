#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>
#include <levos/bitmap.h>

int ext2_read_block(struct filesystem *fs, void *buf, uint32_t block)
{
    uint32_t spb = EXT2_PRIV(fs)->sectors_per_block;

    if (!spb)
        spb ++;

    dev_seek(fs->dev, block * spb);
    fs->dev->read(fs->dev, buf, spb);

    return 0;
}

int ext2_write_block(struct filesystem *fs, void *buf, uint32_t block)
{
    uint32_t spb = EXT2_PRIV(fs)->sectors_per_block;

    //printk("%s: %d\n", __func__, block);

    if (!spb)
        spb ++;

    dev_seek(fs->dev, block * spb);
    fs->dev->write(fs->dev, buf, spb);

    return 0;
}

int ext2_alloc_block(struct filesystem *fs)
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
        if (bgd->num_of_unalloc_block > 0) {
            struct bitmap bm;
            char *buffer;
            size_t found_block;

            /* there is at least one block here */
            buffer = malloc(p->blocksize);
            if (!buffer) {
                free(block_buf);
                return -ENOMEM;
            }

            /* read in the block usage bitmap */
            ext2_read_block(fs, buffer, bgd->block_of_block_usage_bitmap);

            //printk("ext2: blocks in blockgroup %d\n", p->sb.blocks_in_blockgroup);
            /* open the bitmap */
            bitmap_create_using_buffer(p->sb.blocks_in_blockgroup,
                    buffer, &bm);

            /* find a free bit */
            found_block = bitmap_scan_and_flip(&bm, 0, 1, false);
            if (found_block == BITMAP_ERROR) {
                printk("[ext2] CRITICAL: inconsistent block bitmap\n");
                free(block_buf);
                free(buffer);
                /* this is weird, try with the next bgd */
                continue;
            }
            /* we found a block, commit the bitmap */
            //printk("bgd->block_of_block_usage_bitmap: %d\n", bgd->block_of_block_usage_bitmap);
            ext2_write_block(fs, buffer, bgd->block_of_block_usage_bitmap);
            
            /* update the superblock */
            p->sb.unallocatedblocks --;
            ext2_write_superblock(fs);

            /* now update the BGD */
            bgd->num_of_unalloc_block --;
            ext2_write_block(fs, block_buf, p->first_bgd);

            /* clean up */
            free(buffer);
            free(block_buf);

            /* return the block id */
            //printk("ALLOCATED BLOCK %d\n", i * p->sb.blocks_in_blockgroup + found_block + 1);
            return i * p->sb.blocks_in_blockgroup + found_block + 1;
        }
    }
    printk("WARNING: CRITICAL: Couldn't find a free block!\n");
    return -1;
}
