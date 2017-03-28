#include <levos/kernel.h>
#include <levos/palloc.h>
#include <levos/arch.h>
#include <levos/bitmap.h>

static int palloc_total_pages;

/* FIXME: should account for more than 256 MB RAM */

static elem_type palloc_bitmap_bits[16384];
static struct bitmap palloc_bitmap = {
    .bit_cnt = 0,
    .bits = palloc_bitmap_bits,
};

static void
palloc_mark(int id)
{
    bitmap_mark(&palloc_bitmap, id);
}

void
palloc_mark_address(uintptr_t ptr)
{
    bitmap_mark(&palloc_bitmap, ptr / 4096);
}

uintptr_t
palloc_get_pages(int num)
{
    size_t pg;

    pg = bitmap_scan_and_flip(&palloc_bitmap, 0, num, 0);
    if (pg == BITMAP_ERROR)
        panic("Out of physical memory\n");

    return pg * 4096;
}

uintptr_t
palloc_get_page(void)
{
    return palloc_get_pages(1);
}

void
palloc_init(void)
{
    uintptr_t ptr;

    palloc_total_pages = arch_get_total_ram();
    palloc_bitmap.bit_cnt = 65536;
    palloc_bitmap.bits = palloc_bitmap_bits;

    /* mark first 14 MB */
    for (ptr = 0; ptr < 14 * 1024 * 1024; ptr += 4096)
        palloc_mark_address(ptr);

    /* mark VGA RAM */
    for (ptr = 0xB8000; ptr < 0xB8000 + 32 * 1024; ptr += 4096)
        palloc_mark_address(ptr);

	/* mark kernel */
	for (ptr = KERNEL_PHYS_BASE; ptr < KERNEL_PHYS_END; ptr += 4096)
		palloc_mark_address(ptr);
}
