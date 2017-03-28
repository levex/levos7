#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>

int ext2_read_block(struct filesystem *fs, void *buf, uint32_t block)
{
    uint32_t spb = EXT2_PRIV(fs)->sectors_per_block;

    if (!spb)
        spb ++;

    dev_seek(fs->dev, block * spb);
    fs->dev->read(fs->dev, buf, spb);

    return 0;
}
