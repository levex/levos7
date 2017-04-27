#ifndef __LEVOS_MULTIBOOT_H
#define __LEVOS_MULTIBOOT_H

#include <levos/types.h>
#include <levos/compiler.h>

#define MULTIBOOT_SIGNATURE 0x2BADB002

struct multiboot_header {
    uint32_t mb_flags;
    uint32_t mb_mem_lower;
    uint32_t mb_mem_upper;
    uint32_t mb_boot_device;
    char *mb_cmdline;
    uint32_t mb_mods_count;
    uint32_t mb_mods_addr;
    uint8_t  mb_syms[12];
    uint32_t mb_mmap_length;
    uint32_t mb_mmap_addr;
    /* more here */
} __packed;

#endif /* __LEVOS_MULTIBOOT_H */
