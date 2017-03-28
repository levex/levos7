#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>

int ext2_read_inode(struct filesystem *fs, struct ext2_inode *buf, int inode)
{
    struct ext2_priv_data *p = EXT2_PRIV(fs);
    uint32_t bg = (inode - 1) / p->sb.inodes_in_blockgroup;
    uint32_t i = 0;

    char *block_buf = malloc(p->blocksize);
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
