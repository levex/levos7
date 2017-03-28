#include <levos/kernel.h>
#include <levos/page.h>
#include <levos/bitmap.h>
#include <levos/palloc.h>

static int kmap_setup_done = 0;

#ifdef CONFIG_KMAP_USE_BITMAP
  static struct bitmap *kmap_bmap;
#else
  static uint32_t last_kmap_addr;
#endif

void
virt_kmap_init(void)
{
    if (kmap_setup_done)
        panic("kmapping is already setup\n");

#ifdef CONFIG_KMAP_USE_BITMAP
    printk("kmap: using bitmap\n");
    kmap_bmap = bitmap_create(65536);
    if (kmap_bmap == NULL)
        panic("Not enough RAM for kmap\n");
#else
    printk("kmap: using thrashing\n");
    last_kmap_addr = (uint32_t) (VIRT_BASE + 16 * 1024 * 1024);
    printk("kmap: starting at 0x%x\n", last_kmap_addr);
#endif

    printk("kmap: setup done\n");
    kmap_setup_done = 1;
}

void *
kmap_get_free_address(void)
{
    if (!kmap_setup_done)
        panic("too early kmapping\n");

#ifdef CONFIG_KMAP_USE_BITMAP
    size_t id = bitmap_scan_and_flip(kmap_bmap, 0, 1, 0);
    if (id == BITMAP_ERROR)
        panic("Out of kernel virtual space\n");
    return VIRT_BASE + 12 * 1024 * 1024 + id * 4096;
#else
    last_kmap_addr = last_kmap_addr + 0x1000;
    return (void *) (last_kmap_addr - 0x1000);
#endif

}

void *
kmap_get_page(void)
{
    void *vaddr = kmap_get_free_address();
    void *paddr = palloc_get_page();

    //printk("kmap: hello, you are 0x%x\n", __builtin_return_address(0));

    printk("kmap: paddr 0x%x -> vaddr 0x%x\n", paddr, vaddr);

    map_page_kernel(paddr, vaddr, 1);
    __flush_tlb();

    return vaddr;
}
