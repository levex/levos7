#include <levos/kernel.h>
#include <levos/palloc.h>
#include <levos/arch.h>
#include <levos/bitmap.h>

static int palloc_total_pages;

static char palloc_bitmap_bits[8192];

static struct bitmap pre_palloc_bitmap = {
    .bit_cnt = 8192,
    .bits = palloc_bitmap_bits,
};

static struct bitmap *palloc_bitmap = &pre_palloc_bitmap;

static void
palloc_mark(int id)
{
    bitmap_mark(palloc_bitmap, id);
}

void
palloc_mark_address(uintptr_t ptr)
{
    bitmap_mark(palloc_bitmap, ptr / 4096);
}

void
palloc_free_pages(void *addr, int num)
{
    panic_ifnot((int) addr % 4096 == 0);

    bitmap_set_multiple(palloc_bitmap, ((int)addr / 4096), num, 0);
    return;
}

void
palloc_free_page(void *addr)
{
    palloc_free_pages(addr, 1);
}

uintptr_t
palloc_get_pages(int num)
{
    size_t pg;

    pg = bitmap_scan_and_flip(palloc_bitmap, 0, num, 0);
    if (pg == BITMAP_ERROR)
        panic("Out of physical memory\n");

    return pg * 4096;
}

uintptr_t
palloc_get_page(void)
{
    return palloc_get_pages(1);
}

size_t
palloc_get_free(void)
{
    int ret = bitmap_count(palloc_bitmap, 0, palloc_bitmap->bit_cnt, 0);
    //printk("%d pages are marked as 0\n", ret);
    return ret;
}

int
palloc_get_total(void)
{
    return palloc_bitmap->bit_cnt;
}

size_t
palloc_get_used(void)
{
    return palloc_get_total() - palloc_get_free();
}

size_t
palloc_proc_memfree(int pos, void *buf, size_t len, char *__arg)
{
    char __buf[14];
    int actlen;

    memset(__buf, 0, 14);
    itoa(palloc_get_free() * 4096, 10, __buf);
    actlen = strlen(__buf);

    if (pos > actlen)
        return 0;

    if (pos + len > actlen)
        len = actlen - pos;

    memcpy(buf, __buf + pos, len);
    return len;
}

size_t
palloc_proc_memused(int pos, void *buf, size_t len, char *__arg)
{
    char __buf[14];
    int actlen;

    memset(__buf, 0, 14);
    itoa(palloc_get_used() * 4096, 10, __buf);
    actlen = strlen(__buf);

    if (pos > actlen)
        return 0;

    if (pos + len > actlen)
        len = actlen - pos;

    memcpy(buf, __buf + pos, len);
    return len;
}

size_t
palloc_proc_memtotal(int pos, void *buf, size_t len, char *__arg)
{
    char __buf[14];
    int actlen;

    memset(__buf, 0, 14);
    itoa(palloc_get_total() * 4096, 10, __buf);
    actlen = strlen(__buf);

    if (pos > actlen)
        return 0;

    if (pos + len > actlen)
        len = actlen - pos;

    memcpy(buf, __buf + pos, len);
    return len;
}

void
palloc_reinit(void)
{
    DISABLE_IRQ();
    palloc_bitmap = bitmap_create(arch_get_total_ram() / 4096) ;
    if (palloc_bitmap == NULL)
        panic("failed to reinitalize palloc\n");
    memcpy(palloc_bitmap->bits, palloc_bitmap_bits, 8192);
    ENABLE_IRQ();
}

void
palloc_init(void)
{
    uintptr_t ptr;

    memset(palloc_bitmap_bits, 0, 8192);

    //palloc_total_pages = arch_get_total_ram() / 4096;
    palloc_bitmap->bit_cnt = 8192 * 8;
    palloc_bitmap->bits = palloc_bitmap_bits;

    /* mark first 14 MB */
    for (ptr = 0; ptr < 14 * 1024 * 1024; ptr += 4096)
        palloc_mark_address(ptr);

    /* 13MB-14MB will be used for the emergency pagetables */

    /* mark VGA RAM */
    //for (ptr = 0xB8000; ptr < 0xB8000 + 32 * 1024; ptr += 4096)
        //palloc_mark_address(ptr);

	/* mark kernel */
	//for (ptr = KERNEL_PHYS_BASE; ptr < KERNEL_PHYS_END; ptr += 4096)
		//palloc_mark_address(ptr);
}
