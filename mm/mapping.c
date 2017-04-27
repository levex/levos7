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
    f->refc ++;
    map->map_backing = f;
    map->map_refc = 1;
    list_push_back(&map_list, &map->map_list_elem);

    return map;
}

int
mapping_load(struct mapping *map, void *addr, uint32_t offset, int flags)
{
    int len;
    uintptr_t phys;

    //printk("%s: pid %d addr 0x%x offset 0x%x\n",
            //__func__, current_task->pid, addr, offset);

    file_seek(map->map_backing, offset);
    phys = palloc_get_page();

    map_page_curr(phys, addr, 1);

    len = map->map_backing->fops->read(map->map_backing, addr, 4096);
    if (4096 - len)
        memset(addr + len, 0, 4096 - len);

    if (!(flags & VMA_WRITEABLE)) {
        page_t *page = get_page_from_curr(addr);
        pte_mark_read_only(page);
    }

    //printk("mapping finished\n");

    return 0;
}

void
mapping_copy(struct vm_area *dst, struct vm_area *src)
{
    if (src->vma_mapping == NULL)
        return;

    dst->vma_mapping = src->vma_mapping;
    src->vma_mapping->map_refc ++;
    src->vma_mapping->map_backing->refc ++;
}

void
mapping_destroy(struct mapping *map)
{
    if (map->map_refc > 1) {
        map->map_refc --;
        return;
    }

    /* we are the last reference */
    /* TODO: free associated pages */
    free(map);
}

void
mapping_init()
{
    list_init(&map_list);
}
