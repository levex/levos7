#include <levos/kernel.h>
#include <levos/page.h>
#include <levos/vma.h>
#include <levos/spinlock.h>
#include <levos/task.h>
#include <levos/list.h>

struct list map_list;

struct mapping *
mapping_find_or_create(struct file *f)
{
    struct mapping *map = malloc(sizeof(*map));

    /* XXX: this may need duping */
    //vfs_inc_refc(f);
    f = dup_file(f);
    map->map_backing = f;
    map->map_refc = 1;
    //list_push_back(&map_list, &map->map_list_elem);

    return map;
}

int
mapping_load(struct mapping *map, void *addr, uint32_t offset, uint32_t max_len, int flags)
{
    int len = 0;
    uintptr_t phys;

    panic_ifnot((int)addr % 4096 == 0);

    //printk("%s: pid %d addr 0x%x max_len 0x%x offset 0x%x\n",
            //__func__, current_task->pid, addr, max_len, offset);

    file_seek(map->map_backing, offset);

    phys = palloc_get_page();
    map_page_curr(phys, addr, 1);

    memset(addr, 0, 4096);

    if (max_len != 0)
        len = map->map_backing->fops->read(map->map_backing, addr, max_len);

    /*if (4096 - max_len)
        memset(addr + len, 0, 4096 - max_len);*/

    //printk("read %d zero %d bytes\n", len, 4096 - max_len);

    if (!(flags & VMA_WRITEABLE)) {
        page_t *page = get_page_from_curr(addr);
        pte_mark_read_only(page);
    }

    //printk("mapping finished\n");

    return 0;
}

int
new_mapping_load(struct mapping *map, uintptr_t put_loc,
        uintptr_t map_start, uintptr_t map_end)
{
    uintptr_t phys = palloc_get_page();
    size_t read_bytes = (map_end - map_start > 4096) ? 4096 : map_end - map_start;

    read_bytes -= (map_start % 4096);

    //printk("reading bytes 0x%x\n", read_bytes);

    map_page_curr(phys, put_loc, 1);
    memset(put_loc, 0, 4096);

    file_seek(map->map_backing, map_start);
    map->map_backing->fops->read(map->map_backing, put_loc, read_bytes);

    return 0;
}

void
mapping_copy(struct vm_area *dst, struct vm_area *src)
{
    if (src->vma_mapping == NULL) {
        dst->vma_mapping = NULL;
        return;
    }

    dst->vma_mapping = src->vma_mapping;
    src->vma_mapping->map_refc ++;
}

void
mapping_destroy(struct mapping *map)
{
    if (map->map_refc > 1) {
        map->map_refc --;
        return;
    }

    vfs_close(map->map_backing);

    /* we are the last reference */
    /* TODO: free associated pages */
    free(map);
}

void
mapping_init()
{
    list_init(&map_list);
}
