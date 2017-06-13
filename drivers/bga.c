#include <levos/kernel.h>
#include <levos/pci.h>
#include <levos/page.h>
#include <levos/fs.h>

#define MODULE_NAME bga

#define VBE_DISPI_BANK_ADDRESS          0xA0000
#define VBE_DISPI_BANK_SIZE_KB          64

#define VBE_DISPI_MAX_XRES              1024
#define VBE_DISPI_MAX_YRES              768

#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF

#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9

#define VBE_DISPI_ID0                   0xB0C0
#define VBE_DISPI_ID1                   0xB0C1
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_ID3                   0xB0C3
#define VBE_DISPI_ID4                   0xB0C4

#define VBE_DISPI_DISABLED               0x00
#define VBE_DISPI_ENABLED                0x01
#define VBE_DISPI_GETCAPS                0x02
#define VBE_DISPI_8BIT_DAC               0x20
#define VBE_DISPI_LFB_ENABLED            0x40
#define VBE_DISPI_NOCLEARMEM             0x80

#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000

struct fb_file_priv {
    uint32_t lfb;
    uint32_t lfb_size;
};

static const struct pci_ident bga_pci_idents[] = {
    PCI_IDENT(0x1234, 0x1111),
    PCI_END_IDENT,
};

int fb_file_write(struct file *f, void *_buf, size_t count)
{
    struct fb_file_priv *p = f->priv;

    if (f->fpos > p->lfb_size)
        return 0;

    if (f->fpos + count > p->lfb_size)
        count = p->lfb_size - f->fpos;

    //printk("%s: fpos %d from 0x%x count %d\n", __func__, f->fpos, _buf, count);

    memcpy(p->lfb + f->fpos, _buf, count);

    f->fpos += count;

    return count;
}

int fb_file_read(struct file *f, void *_buf, size_t count)
{
    return 0;
}

int fb_file_fstat(struct file *f, struct stat *st)
{
    st->st_mode = S_IFCHR;

    return 0;
}

int
fb_file_truncate(struct file *f, int pos)
{
    return 0;
}

int
fb_file_close(struct file *f)
{
    free(f->full_path);
    free(f);
}

struct __fb_arg {
    uint32_t x;
    uint32_t y;
};

int
fb_file_ioctl(struct file *f, int cmd, int arg)
{
    if (cmd == 0x1337) {
        struct __fb_arg *a = arg;
        bga_set_video(a->x, a->y, 32, 1, 1);
        return 0;
    }

    return -EINVAL;
}

struct file_operations fb_fops = {
    .read = fb_file_read,
    .write = fb_file_write,
    .fstat = fb_file_fstat,
    .close = fb_file_close,
    .truncate = fb_file_truncate,
    .ioctl = fb_file_ioctl,
};

struct file fb_base_file = {
    .fops = &fb_fops,
    .fs = NULL,
    .fpos = 0,
    .isdir = 0,
    .respath = "fb",
    .full_path = "/dev/fb",
};

void bga_pci_init() { ; }
int bga_pci_probe(struct pci_device *pdev) { return 0; }
int bga_pci_attach(struct pci_device *pdev) {
    uint32_t bar0 = pci_dev_config_read32(pdev, 0x10);
    uint32_t lfb = PG_RND_DOWN(bar0);

    //lfb = VBE_DISPI_LFB_PHYSICAL_ADDRESS;

    mprintk("BAR 0 is at 0x%x\n", bar0);


    /* map the FB */
    for (uint32_t addr = lfb; addr < lfb + 8 * 1024 * 1024; addr += 0x1000)
        map_page_kernel(addr, addr, 0);

    //memsetl(lfb, 0x00ff4488, 8 * 1024 * 1024);
    
    struct fb_file_priv *priv = malloc(sizeof(struct fb_file_priv));
    priv->lfb = lfb;
    priv->lfb_size = 8 * 1024 * 1024; /* XXX */

    fb_base_file.priv = priv;


    __flush_tlb();
}

uint32_t
bga_get_lfb()
{
    return ((struct fb_file_priv *)fb_base_file.priv)->lfb;
}

struct pci_driver bga_pci_driver = {
    .name = "Bochs Graphics Adapter",
    .init = bga_pci_init,
    .probe = bga_pci_probe,
    .attach = bga_pci_attach,
    .idents = (void *) &bga_pci_idents,
};

void
bga_write_reg(uint16_t idx, uint16_t val)
{
    outportw(VBE_DISPI_IOPORT_INDEX, idx);
    outportw(VBE_DISPI_IOPORT_DATA, val);
}

uint16_t
bga_read_reg(uint16_t idx)
{
    outportw(VBE_DISPI_IOPORT_INDEX, idx);
    return inportw(VBE_DISPI_IOPORT_DATA);
}

int
bga_available(void)
{
    mprintk("version read: 0x%x\n", bga_read_reg(VBE_DISPI_INDEX_ID));
    return (bga_read_reg(VBE_DISPI_INDEX_ID) >= 0xB0C0);
}


void
bga_set_video(int width, int height, int bit_depth, int uselfb, int clearvidmem)
{
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write_reg(VBE_DISPI_INDEX_ID, VBE_DISPI_ID4);
    mprintk("fix up version: 0x%x\n", bga_read_reg(VBE_DISPI_INDEX_ID));
    bga_write_reg(VBE_DISPI_INDEX_BPP, bit_depth);
    bga_write_reg(VBE_DISPI_INDEX_XRES, width);
    bga_write_reg(VBE_DISPI_INDEX_YRES, height);
    bga_write_reg(VBE_DISPI_INDEX_BANK, 0);
    bga_write_reg(VBE_DISPI_INDEX_VIRT_WIDTH, width);
    bga_write_reg(VBE_DISPI_INDEX_VIRT_HEIGHT, 4096);
    bga_write_reg(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write_reg(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED |
        (uselfb ? VBE_DISPI_LFB_ENABLED : 0) |
        (clearvidmem ? 0 : VBE_DISPI_NOCLEARMEM));
}

uint8_t test_page[4096] __page_align;


int
bga_init()
{
    if (!bga_available()) {
        mprintk("no device found\n");
        return -ENODEV;
    }

    mprintk("initialized\n");

    //bga_set_video(1024, 768, 32, 1, 1);

    return 0;
}
