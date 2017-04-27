#include <levos/kernel.h>
#include <levos/page.h>
#include <levos/vma.h>
#include <levos/spinlock.h>
#include <levos/task.h>

bool
vm_area_less(const struct list_elem *a, const struct list_elem *b, void *aux)
{
    struct vm_area *vma_a = list_entry(a, struct vm_area, vma_list_elem);
    struct vm_area *vma_b = list_entry(b, struct vm_area, vma_list_elem);

    return vma_a->vma_start < vma_b->vma_start;
}

bool
vm_area_check_overlap(struct vm_area *a, struct vm_area *b)
{
    return a->vma_end > b->vma_start;
}

bool
vm_area_overlaps(struct vm_area *vma, struct list *vm_areas)
{
    bool has_overlap = false;
    struct vm_area *other_vma;

    if (list_front(vm_areas) != &vma->vma_list_elem) {
        other_vma = list_entry(list_prev(&vma->vma_list_elem), struct vm_area, vma_list_elem);
        has_overlap |= vm_area_check_overlap(other_vma, vma);
    }

    if (list_back(vm_areas) != &vma->vma_list_elem) {
        other_vma = list_entry(list_next(&vma->vma_list_elem), struct vm_area, vma_list_elem);
        has_overlap |= vm_area_check_overlap(vma, other_vma);
    }

    return has_overlap;
}

struct vm_area *
vm_area_create_insert(uint32_t vaddr_start, uint32_t vaddr_end,
                        struct task *task, int flags)
{
    panic_ifnot(vaddr_start + 4096 <= vaddr_end);
    struct vm_area *vma;

    vma = calloc(1, sizeof(struct vm_area));
    if (!vma)
        return NULL;

    vma->vma_start = vaddr_start;
    vma->vma_end = vaddr_end;
    vma->vma_flags = flags;

    spin_lock(&task->vm_lock);

    list_insert_ordered(&task->vma_list, &vma->vma_list_elem, vm_area_less, NULL);

    if (vm_area_overlaps(vma, &task->vma_list)) {
        list_remove(&vma->vma_list_elem);
        free(vma);
        vma = NULL;
    }

    spin_unlock(&task->vm_lock);

    return vma;
}

struct vm_area *
vm_area_create_insert_curr(uint32_t s, uint32_t e, int f)
{
    return vm_area_create_insert(s, e, current_task, f);
}

int
vma_set_mapping(struct vm_area *vma, struct file *f, uint32_t offset, uint32_t len)
{
    panic_ifnot(vma->vma_mapping == NULL);

    if (f == NULL) {
        /* anonymous mapping */
        vma->vma_flags |= VMA_ANONYMOUS;
        vma->vma_mapping = NULL;
    } else {
        /* backed by a file */
        vma->vma_flags &= ~VMA_ANONYMOUS;
        vma->vma_mapping = mapping_find_or_create(f);
        vma->vma_mapping_offset = offset;
        vma->vma_mapping_length = len;
    }

    return 0;
}

struct vm_area *
vma_find(struct list *vm_areas, uint32_t addr)
{
    struct list_elem *e;
    struct vm_area *vma;

    //vma_dump(current_task);

    list_foreach_raw(vm_areas, e) {
        vma = list_entry(e, struct vm_area, vma_list_elem);

        //printk("COMPARE 0x%x with [0x%x - 0x%x]\n",
                //addr, vma->vma_start, vma->vma_end);
        if (vma->vma_start <= addr && addr < vma->vma_end)
            return vma;
    }
    
    return NULL;
}

int
vma_load(struct vm_area *vma, uint32_t addr)
{
    if (vma->vma_mapping) {
        uint32_t real_offset = vma->vma_mapping_offset + (addr - vma->vma_start);
        return mapping_load(vma->vma_mapping, addr, real_offset, vma->vma_flags);
    } else {
        uintptr_t phys = palloc_get_page();
        map_page_curr(phys, addr, 1);
        memset(addr, 0, 4096);
        return 0;
    }
}

int
vma_handle_pagefault(struct task *task, uint32_t req_addr)
{
    uint32_t req_addr_a = PG_RND_DOWN(req_addr);

    //printk("%s: req_addr 0x%x\n", __func__, req_addr);

    struct vm_area *vma = vma_find(&task->vma_list, req_addr_a);
    if (!vma)
        return 1;

    return vma_load(vma, req_addr_a);
}

void
vma_dump(struct task *task)
{
    struct list_elem *elem;

    spin_lock(&task->vm_lock);

    list_foreach_raw(&task->vma_list, elem) {
        struct vm_area *vma = list_entry(elem, struct vm_area, vma_list_elem);

        printk("VMA [0x%x - 0x%x] offset: 0x%x %s\n", vma->vma_start,
                vma->vma_end, vma->vma_mapping_offset, "anonymous");
    }

    spin_unlock(&task->vm_lock);
}

void
copy_vmas(struct task *new, struct task *old)
{
    struct list_elem *elem;

    //printk("%s\n", __func__);

    list_init(&new->vma_list);
    spin_lock_init(&new->vm_lock);

    list_foreach_raw(&old->vma_list, elem) {
        struct vm_area *vma = list_entry(elem, struct vm_area, vma_list_elem);
        struct vma_area *vma_new
            = vm_area_create_insert(vma->vma_start, vma->vma_end, new, vma->vma_flags);

        mapping_copy(vma_new, vma);
    }
}

void
vma_destroy(struct vm_area *vma)
{
    if (vma->vma_mapping)
        mapping_destroy(vma->vma_mapping);

    free(vma);
}

void
vma_unload_all(struct task *task)
{
    struct list_elem *elem;

    list_foreach_raw(&task->vma_list, elem) {
        struct vm_area *vma = list_entry(elem, struct vm_area, vma_list_elem);

        vma_destroy(vma);
        list_remove(elem);
    }

    //printk("unloaded all VMAs: %d left\n", list_size(&task->vma_list));
}
