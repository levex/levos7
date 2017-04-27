#ifndef __LEVOS_VMA_H
#define __LEVOS_VMA_H

#include <levos/fs.h>
#include <levos/types.h>
#include <levos/kernel.h>
#include <levos/list.h>

#define VMA_ANONYMOUS (1 << 0)
#define VMA_GROWSDOWN (1 << 1)
#define VMA_WRITEABLE (1 << 2)

struct mapping {
    struct file *map_backing;

    int map_refc;

    struct list_elem map_list_elem;
};

struct vm_area {
    uint32_t vma_start;
    uint32_t vma_end;

    uint32_t vma_mapping_offset;
    uint32_t vma_mapping_length;
    struct mapping *vma_mapping;

    int vma_flags;

    struct list_elem vma_list_elem;
};

#endif /* __LEVOS_VMA_H */
