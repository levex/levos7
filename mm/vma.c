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
vm_area_create_insert(uint32_t vaddr_start, uint32_t vaddr_offset, uint32_t vaddr_end,
                        struct task *task, int flags)
{
    panic_ifnot(vaddr_start + 4096 <= vaddr_end);
    struct vm_area *vma;

    vma = calloc(1, sizeof(struct vm_area));
    if (!vma)
        return NULL;

    vma->vma_start = vaddr_start;
    vma->vma_end = vaddr_end;
    vma->vma_offset = vaddr_offset;
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
vm_area_create_insert_curr(uint32_t s, uint32_t o, uint32_t e, int f)
{
    return vm_area_create_insert(s, o, e, current_task, f);
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
        //mapping_set(vma->vma_mapping, offset, len);
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
    int rc;
    uint32_t offset, map_begin, map_end;

    panic_ifnot(addr % 4096 == 0);

    offset = addr - vma->vma_start;
    map_begin = vma->vma_mapping_offset + offset;
    map_end = vma->vma_mapping_offset + vma->vma_mapping_length;

    if (vma->vma_mapping) {
        if (map_begin >= PG_RND_UP(map_end))
            goto map_zero;


        int read_bytes = vma->vma_mapping_length;
        int zero_bytes = vma->vma_end - read_bytes - vma->vma_start;

        panic_ifnot(addr >= vma->vma_start && addr <= vma->vma_start + read_bytes + zero_bytes);

        int max_len = read_bytes - offset;
        if (max_len < 0)
            max_len = 0;
        else if (max_len > 0x1000)
            max_len = 0x1000;


        rc = mapping_load(vma->vma_mapping, addr, map_begin, max_len, vma->vma_flags);
        //rc = new_mapping_load(vma->vma_mapping, addr, map_begin, map_end);
        //if (map_begin == 0) {
            //printk("FIRST PAGE\n");
            //memset(addr, 0, vma->vma_offset);
        //}
#if 0

        printk("f_off: 0x%x vaddr: 0x%x f_sz: 0x%x m_sz: 0x%x\n",
                vma->vma_mapping_offset + vma->vma_offset, /* f_off */
                addr + map_begin, /* vaddr */
                read_bytes > 4096 ? 4096 : read_bytes,  /* f_sz */
                read_bytes + zero_bytes); /* m_sz */

        struct mapping *map = vma->vma_mapping;
        rc = 0;

        map_page_curr(palloc_get_page(), addr + map_begin, 1);

        file_seek(map->map_backing, vma->vma_mapping_offset + offset - vma->vma_offset));

        map->map_backing->fops->read(map->map_backing,
                addr + map_begin,
                read_bytes > 4096 ? 4096 : read_bytes);
#endif
        return rc;
    } else {
map_zero: ;
        uintptr_t phys = palloc_get_page();
        map_page_curr(phys, addr, 1);
        memset(addr, 0, 4096);
        //printk("ZERO FILLING: addr 0x%x phys 0x%x\n", addr, phys);
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

    /* reserve VMAs can't be pagefaulted in */
    if (vma->vma_flags & VMA_RESERVED)
        return 1;

    /* FIXME: figure out what it was trying to do */

    return vma_load(vma, req_addr_a);
}

void
vma_dump(struct task *task)
{
    struct list_elem *elem;

    spin_lock(&task->vm_lock);

    list_foreach_raw(&task->vma_list, elem) {
        struct vm_area *vma = list_entry(elem, struct vm_area, vma_list_elem);

        printk("VMA [0x%x - 0x%x] map_offset: 0x%x s_off: 0x%x map_length: 0x%x flags: 0x%x %s%s\n",
                vma->vma_start, vma->vma_end,
                vma->vma_mapping_offset, 
                vma->vma_offset,
                vma->vma_mapping_length, 
                vma->vma_flags,
                vma->vma_mapping ?  "file: " : "anonymous",
                vma->vma_mapping ? vma->vma_mapping->map_backing->full_path : "");
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
        struct vm_area *vma_new
            = vm_area_create_insert(vma->vma_start, vma->vma_offset, vma->vma_end, new, vma->vma_flags);
        vma_new->vma_mapping_offset = vma->vma_mapping_offset;
        vma_new->vma_mapping_length = vma->vma_mapping_length;

        mapping_copy(vma_new, vma);
    }
}

void
vma_try_prefault(struct task *task, uint32_t addr, uint32_t len)
{
    struct vm_area *vma;
    uint32_t base = PG_RND_DOWN(addr);

//    printk("%s: addr 0x%x len %d base 0x%x again 0x%x\n", __func__, addr, len, base,
            //PG_RND_DOWN(base));

    for (base = PG_RND_DOWN(addr); base < PG_RND_UP(addr + len); base += 4096) {
        page_t *page = get_page_from_curr(base);
        if (!page || (page && ((*page & (1 << 0)) == 0))) {
            vma = vma_find(&task->vma_list, base);
            if (vma) {
                //printk("%s: VMA load base 0x%x\n", __func__, base);
                vma_load(vma, base);
            } else {
                /* This is probably the data segment: TODO convert to VMA */
                //printk("OMG: THIS IS VERY SAD for address base 0x%x\n", base);
                //dump_stack(8);
                //vma_dump(task);
            }
        }
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
vma_init(struct task *task)
{
    struct vm_area *stack_vma;

    list_init(&task->vma_list);

    stack_vma = vm_area_create_insert(VIRT_BASE - (0x1000 * 1000), 0, VIRT_BASE,
                        task, VMA_WRITEABLE | VMA_STACK);

    if (stack_vma == NULL)
        printk("CRITICAL: failed to create VMA for the stack\n");

//    printk("created VMA subsystem for pid %d\n", task->pid);
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

uint32_t
vma_find_free_region(struct task *task, size_t len)
{
    struct list_elem *elem;
    struct vm_area *prev = NULL;

    list_foreach_raw(&task->vma_list, elem) {
        struct vm_area *vma = list_entry(elem, struct vm_area, vma_list_elem);
        if (prev == NULL) {
            prev = vma;
            continue;
        }

        if (vma->vma_start - prev->vma_end > len)
            return prev->vma_end;

        prev = vma;
    }

    return -1;
}

struct vm_area *
do_mmap_fixed(struct file *f, void *addr, size_t len, size_t offset)
{
    struct vm_area *vma;
    int rc;

    vma = vm_area_create_insert(addr, 0, addr + len, current_task, VMA_MMAP);
    if (!vma)
        return NULL;

    rc = vma_set_mapping(vma, f, offset, len);
    if (rc) {
        vma_destroy(vma);
        return rc;
    }

    return vma;
}

void *
do_mmap(void *addr, size_t len, int prot, int flags, struct file *f,
        size_t offset)
{
    struct vm_area *vma = NULL;

    if (flags & MAP_FIXED
            && addr == NULL)
        return -EINVAL;

    if (len == 0)
        return -EINVAL;

    if (flags & MAP_FIXED) {
        vma = do_mmap_fixed(f, addr, len, offset);
        if (!vma)
            return -ENOMEM;
    } else {
        /* find a suitable region */
        uint32_t base_addr = vma_find_free_region(current_task, len);
        if (base_addr == -1)
            return -ENOMEM;

        /* XXX: there is a possible race here */

        vma = do_mmap_fixed(f, base_addr, len, offset);
        if (!vma)
            return -ENOMEM;

        addr = base_addr;
    }

    /* apply prot */
    if (prot & PROT_WRITE)
        vma->vma_flags |= VMA_WRITEABLE;

    /* FIXME: this is noop for now */
    if (!(prot & PROT_READ))
        vma->vma_flags |= VMA_NOREAD;

    /* fixup flags */
    if (flags & MAP_SHARED)
        vma->vma_flags |= VMA_SHARED;

    return addr;
}
