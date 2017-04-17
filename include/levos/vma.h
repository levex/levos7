#ifndef __LEVOS_VMA_H
#define __LEVOS_VMA_H

#include <levos/fs.h>
#include <levos/types.h>
#include <levos/kernel.h>

struct vm_area {
    uint32_t vma_start;
    uint32_t vma_end;

    int vma_flags;

    struct list_elem vma_list_elem;
};

struct mapping {
    struct file *map_backing;
    int map_refc;

    struct list_elem map_list_elem;
};

#endif /* __LEVOS_VMA_H */
