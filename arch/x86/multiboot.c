#include <levos/kernel.h>
#include <levos/multiboot.h>
#include <levos/page.h>

extern int __x86_total_ram;

void
multiboot_handle(struct multiboot_header *_hdr)
{
    struct multiboot_header *hdr = (void *) _hdr + 0xC0000000;

    if (hdr->mb_flags & (1 << 0)) {
        __x86_total_ram = hdr->mb_mem_upper * 1024;
    }
}
