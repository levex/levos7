#ifndef __LEVOS_VMA_H
#define __LEVOS_VMA_H

#include <levos/fs.h>
#include <levos/types.h>
#include <levos/kernel.h>
#include <levos/list.h>

/* sys/mman.h */
#define PROT_EXEC (1 << 0)
#define PROT_WRITE (1 << 1)
#define PROT_READ (1 << 2)
#define PROT_NONE 0

#define MAP_SHARED (1 << 0)
#define MAP_PRIVATE (1 << 1)
#define MAP_FIXED   (1 << 2)
#define MAP_ANON    (1 << 3)
#define MAP_ANONYMOUS MAP_ANON

#define MAP_FAILED ((void *)-1)
/* * */

#define VMA_ANONYMOUS (1 << 0) /* this is an ANONYMOUS VMA, swap backing */
#define VMA_GROWSDOWN (1 << 1) /* this VMA can fault down */
#define VMA_STACK VMA_GROWSDOWN
#define VMA_WRITEABLE (1 << 2) /* this VMA is writeable */
#define VMA_MMAP      (1 << 3) /* this is a mmap(2) VMA */
#define VMA_NOREAD    (1 << 4) /* this VMA is not readable, send sig */
#define VMA_SHARED    (1 << 5) /* changes to this VMA are reflected in other VMAs */
#define VMA_RESERVED  (1 << 6) /* faults to this page result in SIGSEGV */

struct mapping {
    struct file *map_backing;

    int map_refc;

    struct list_elem map_list_elem;
};

struct vm_area {
    uint32_t vma_start;
    uint32_t vma_offset;
    uint32_t vma_end;

    uint32_t vma_mapping_offset;
    uint32_t vma_mapping_length;
    struct mapping *vma_mapping;

    int vma_flags;

    struct list_elem vma_list_elem;
};

#endif /* __LEVOS_VMA_H */
