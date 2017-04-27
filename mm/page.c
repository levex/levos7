#include <levos/kernel.h>
#include <levos/compiler.h>
#include <levos/page.h>
#include <levos/arch.h>
#include <levos/task.h>
#include <levos/palloc.h>
#include <levos/string.h>

pde_t kernel_pgd[1024] __page_align;

static int __in_pagefault;

/* TODO: clean up */
int pte_is_cow(page_t p);

static page_t kernel_pgt[1024] __page_align; /* 768 */
static page_t heap_1_pgt[1024] __page_align; /* 769 */
static page_t heap_2_pgt[1024] __page_align; /* 770 */
static page_t kernel_virt_pgt[1024] __page_align; /* IDK FIXME */

inline int pde_index(uint32_t addr)
{
	return addr >> 22;
}

inline uintptr_t pte_index(uint32_t addr)
{
	return (uintptr_t) ((addr / 4096) % 1024);
}

page_t create_pte(uint32_t phys_addr, int user, int rw)
{
	phys_addr = phys_addr >> 12;

    //user = rw = 1;

	return 0                             |
		   (1 << PTE_PRESENT_SHIFT)      |
		   (rw << PTE_RW_SHIFT)          |
           (user << PTE_USER_SHIFT)      |
		   (1 << PTE_WRITETHRU_SHIFT)    |
		   (0 << PTE_CACHE_SHIFT)        |
           (0 << PTE_ACCESS_SHIFT)       |
           (0 << PTE_DIRTY_SHIFT)        &
           (PTE_ZERO_MASK)               |
           (0 << PTE_GLOB_SHIFT)         &
           (PTE_AVAIL_MASK)              |
           (phys_addr << PTE_ADDR_SHIFT);
}

inline pde_t create_pde(uint32_t phys_addr, int user, int rw)
{
	phys_addr = phys_addr >> 12;

    //user = rw = 1;

	return 0                             |
		   (1    << PDE_PRESENT_SHIFT)   |
           (rw   << PDE_RW_SHIFT)        |
           (user << PDE_USER_SHIFT)      |
		   (1    << PDE_WRITETHRU_SHIFT) |
           (0    << PDE_CACHE_SHIFT)     |
           (0    << PDE_ACCESS_SHIFT)    |
		   (0    << PDE_ZERO_SHIFT)      |
           (0    << PDE_SIZE_SHIFT)      |
		   (0    << PDE_IGNORE_SHIFT)    &
		   (PDE_AVAIL_MASK)              |
		   (phys_addr << PDE_ADDR_SHIFT);
}

void __noreturn
do_kernel_pagefault(struct pt_regs *regs, uint32_t cr2)
{
    DISABLE_IRQ();
    printk("--[cut here]--\n");
    printk("Page fault (#PF) in the kernel\n");
    printk("Dumping registers:\n");
    printk("cs=%x ds=%x es=%x fs=%x gs=%x ss=%x\n",
            regs->cs, regs->ds, regs->es, regs->fs, regs->gs, regs->ss);
    printk("eax=%x ebx=%x ecx=%x edx=%x\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    printk("esi=%x edi=%x\n",
            regs->esi, regs->edi);
    printk("esp=%x eip=%x eflags=%x\n", regs->esp, regs->eip, regs->eflags);
    printk("vector=%x err=%x\n", regs->vec_no, regs->error_code);
    asm volatile("mov %%cr2, %0":"=r"(cr2));
    printk("cr2=0x%x\n", cr2);
    if (current_task) {
        printk("while executing pid=%d\n", current_task->pid);
        printk("irq stack [0x%x - 0x%x]\n", current_task->irq_stack_bot, current_task->irq_stack_top);
    }

    panic("Unable to handle kernel paging request\n");
}

page_t *
get_page_from_pgd(pagedir_t pgd, uint32_t vaddr)
{
    int ipde, ipte;
    pde_t *pdes = pgd, pde;
    page_t *ptes;

    ipde = pde_index(vaddr);
    ipte = pte_index(vaddr);

    //printk("%s: ipde: %d ipte: %d\n", __func__, ipde, ipte);

    pde = pdes[ipde];
    pde >>= PDE_ADDR_SHIFT;
    pde <<= 12;
    if (pde == 0)
        return 0;
    pde += VIRT_BASE;
    ptes = (page_t *)pde;
    return &ptes[ipte];
}

page_t *
get_page_from_curr(uint32_t vaddr)
{
    if (current_task && current_task->mm) {
        //printk("fetching page from current\n");
        return get_page_from_pgd(current_task->mm, vaddr);
    }
    else return get_page_from_pgd(kernel_pgd, vaddr);
}

void
do_cow(uint32_t cr2)
{
    char *buffer = malloc(4096);
    uintptr_t p_np;
    uintptr_t the_page = PG_RND_DOWN(cr2);
    if (!buffer)
        send_signal(current_task, SIGSEGV);

    memcpy(buffer, (void *) the_page, 4096);

    p_np = palloc_get_page();
    replace_page(current_task->mm, the_page, create_pte(p_np, 1, 1));

    page_t *page = get_page_from_curr(cr2);
    pte_mark_writeable(page);

    memcpy((void *) the_page, buffer, 4096);

    free(buffer);

    return;
}

int
in_pagefault()
{
    return __in_pagefault;
}

void
do_user_pagefault(page_t *page, struct pt_regs *regs, uint32_t cr2)
{
    //printk("page fault in pid %d at cr2 = 0x%x\n", current_task->pid, cr2);
    //send_signal(current_task, SIGSEGV);
    
    __in_pagefault = 1;

    //printk("%s: regs: 0x%x\n", __func__, regs);
    //dump_registers(regs);
    //current_task->sys_regs = regs;

    /* this is likely a nullptr exception */
    if (cr2 < 4096) {
        printk("unable to handle null dereference at 0x%x\n", cr2);
        dump_registers(regs);
        vma_dump(current_task);
        send_signal(current_task, SIGSEGV);
    }

    if ((page && !*page) || !page) {
        int rc = vma_handle_pagefault(current_task, cr2);
        if (rc) {
            printk("unable to handle a missing page at 0x%x!\n", cr2);
            dump_registers(regs);
            vma_dump(current_task);
            send_signal(current_task, SIGSEGV);
        }

        return;
    }

    printk("unable to handle user paging (PTE 0x%x) permission error at 0x%x\n", *page, cr2);
    /* we couldn't handle it, sigsegv */
    send_signal(current_task, SIGSEGV);
    __not_reached();
}

void
handle_pagefault(struct pt_regs *regs)
{
    int ipde, ipte;
    uint32_t cr2;
    page_t *page;

    asm volatile("mov %%cr2, %0":"=r"(cr2));

    ///printk("--- pagefault ---\n");
    //dump_registers(regs);

    if (regs->eip == ret_from_signal) {
        ret_from_signal();
        return;
    }

    current_task->sys_regs = regs;

    /* if a COW page is written then fetch new page and map */
    page = get_page_from_curr(PG_RND_DOWN(cr2));
    if (page && pte_is_cow(*page)) {
        do_cow(cr2);
        return;
    }

    if ((unsigned long) regs->eip > (unsigned long) VIRT_BASE)
        do_kernel_pagefault(regs, cr2);

    do_user_pagefault(page, regs, cr2);
}

void __flush_tlb(void)
{
    asm volatile("movl %%cr3, %%eax; movl %%eax, %%cr3":::"eax","memory");
}

int
map_page(pagedir_t pgd, uint32_t phys_addr, uint32_t virt_addr, int perm)
{
    pagetable_t pgt;
    //perm = 1;

    /* first find the pagetable index */
    int ipde = pde_index(virt_addr);
    int ipte = pte_index(virt_addr);
    panic_on(ipde > 1024, "invalid %s (ipde > 1024)", __func__);
    panic_on(ipte > 1024, "invalid %s (ipte > 1024)", __func__);

//    printk("mapping page 0x%x to v 0x%x (%d:%d)\n",
//            phys_addr, virt_addr, ipde, ipte);

    /* check if the page table exists */
    if ((pgt = (pagetable_t) pgd[ipde]) != 0) {
        pgt = (pagetable_t) (((int)pgt >> PDE_ADDR_SHIFT << 12) + VIRT_BASE);

        /* if the page is already mapped then bail */
        /* if (pgt[ipte] != 0)
            return 1;*/

        /* map the page */
        pgt[ipte] = create_pte(phys_addr, perm, 1);
    } else {
        /* the page table doesn't exist, get one */
        pagetable_t pgt = na_malloc(0x1000, 0x1000);
        panic_on((int)pgt % 4096, "pagetable allocated is NOT page aligned\n");
        panic_on(!pgt, "not enough memory to %s\n", __func__) + 0x1000;
        memset(pgt, 0, 4096);

        /* map the page */
        pgt[ipte] = create_pte(phys_addr, perm, 1);
        /* map the page table */
        pgd[ipde] = create_pde(kv2p(pgt), perm, 1);
    }
    __flush_tlb();

    return 0;
}

int
page_mapped(pagedir_t pgd, uint32_t virt_addr)
{
    int ipde = pde_index(virt_addr);
    int ipte = pte_index(virt_addr);
    pagetable_t pgt;

    if ((pgt = (pagetable_t) pgd[ipde]) != 0) {
        pgt = (pagetable_t) (((int)pgt >> PDE_ADDR_SHIFT << 12) + VIRT_BASE);
        return pgt[ipte] != 0;
    }
    return 0;
}

int
page_mapped_curr(uint32_t virt_addr)
{
    if (!current_task || !current_task->mm)
        panic("%s: invalid call\n", __func__);

    return page_mapped(current_task->mm, virt_addr);
}

int
map_page_curr(uint32_t p, uint32_t v, int perm)
{
    extern struct task *current_task;

    if (!current_task)
        panic("invalid mappagecurr\n");

    if (current_task->mm)
        return map_page((pagedir_t) current_task->mm, p, v, perm);
    else
        return map_page(kernel_pgd, p, v, perm);
}

int
map_page_kernel(uint32_t p, uint32_t v, int perm)
{
    int ret = map_page(kernel_pgd, p, v, perm);
    __flush_tlb();
    return ret;
}

pagedir_t
new_page_directory(void)
{
    return copy_page_dir(kernel_pgd);

    /* allocate space for the new page directory */
    pde_t *ret = kmap_get_page();
    memset(ret, 0, sizeof(kernel_pgd));

    ret[pde_index(VIRT_BASE)] = create_pde(kv2p(kernel_pgt), 0, 1);
    ret[pde_index(VIRT_BASE + 4 * 1024 * 1024)]
            = create_pde(kv2p(heap_1_pgt), 0, 1);
    ret[pde_index(VIRT_BASE + 8 * 1024 * 1024)]
            = create_pde(kv2p(heap_2_pgt), 0, 1);

    return (pagedir_t) ret;
}

pde_t
__copy_pt(pde_t orig)
{
    page_t *new_addr = na_malloc(0x1000, 0x1000);
    if (!new_addr)
        panic("ENOMEM");

    void *orig_addr = (void *) (((orig >> PDE_ADDR_SHIFT) << 12) + VIRT_BASE);
    //printk("orig 0x%x orig_addr 0x%x\n", orig, orig_addr);
    memcpy(new_addr, orig_addr, 0x1000);

    return create_pde(kv2p(new_addr), 1, 1);
}

pagedir_t
copy_page_dir(pagedir_t orig)
{
    pde_t *ret = na_malloc(0x1000, 0x1000);
    if (!ret) {
        panic("UNABLE TO CLONE\n");
    }
    memset(ret, 0, 0x1000);

    memcpy(ret, orig, 0x1000);
    for (int i = 0; i < 768; i ++) {
        if (ret[i] != 0) {
            //printk("creating new pde for vaddr 0x%x-0x%x\n", i * 4096 * 1024, (i + 1) * 4096 * 1024);
            ret[i] = __copy_pt(ret[i]);
        }
    }
    return ret;
}

void
mark_all_user_pages_cow(pagedir_t pgd)
{
    int i, j;

    /* loop throught the pagetables */
    for (i = 0; i < 768; i ++) {
        /* if this pagetable exists, then mark it as R/W */
        if (pgd[i] != 0) {
            uint32_t *pde_addr = (void *) VIRT_BASE + ((pgd[i] >> PDE_ADDR_SHIFT) << 12);
            pde_mark_writeable(pde_addr);

            /* now loop through its pages */
            for (j = 0; j < 1024; j ++) {
                if (!pte_present(pde_addr[j]) || !pte_writeable(pde_addr[j]))
                    continue;

                pte_mark_read_only(&pde_addr[j]);
                pte_mark_cow(&pde_addr[j]);
            }
        }
    }
}

void
map_unload_user_pages(pagedir_t pgd)
{
    /* 767 because the stack needs not be unmapped, we reuse it */
    for (int i = 0; i < 767; i++) {
        if (pgd[i] != 0) {
            //printk("unloaded page table 0x%x - 0x%x\n", i * 4 * 1024 * 1024, (i + 1) * 4096 * 1024);
            pgd[i] = 0;
        }
    }
    __flush_tlb();
}

int
pte_present(page_t p)
{
    return p & (1 << PTE_PRESENT_SHIFT);
}

void
pte_mark_read_only(page_t *p)
{
    *p &= ~(1 << PTE_RW_SHIFT);
}

void
pte_mark_writeable(page_t *p)
{
    *p |= (1 << PTE_RW_SHIFT);
}

int
pte_writeable(page_t p)
{
    return p & (1 << PTE_RW_SHIFT);
}

void
pte_mark_cow(page_t *p)
{
    *p |= 1 << PTE_COW_SHIFT;
}

int
pte_is_cow(page_t p)
{
    return p & (1 << PTE_COW_SHIFT);
}

int
pde_present(pde_t p)
{
    return p & (1 << PDE_PRESENT_SHIFT);
}

void
pde_mark_read_only(pde_t *p)
{
    *p &= ~(1 << PDE_RW_SHIFT);
}

void
pde_mark_writeable(pde_t *p)
{
    *p |= (1 << PDE_RW_SHIFT);
}

void
pde_mark_cow(pde_t *p)
{
    *p |= 1 << PDE_COW_SHIFT;
}

int
pde_is_cow(pde_t p)
{
    return p & (1 << PDE_COW_SHIFT);
}

void
replace_page(pagedir_t pgd, uint32_t virt_addr, page_t entry)
{
    page_t *ptr = get_page_from_pgd(pgd, virt_addr);
    *ptr = entry;
}


pagedir_t
copy_page_dir_fork(pagedir_t orig)
{
    pde_t *ret = copy_page_dir(orig);
    return ret;
}

void
paging_init(void)
{
    uint32_t i, j;

    DISABLE_IRQ();
    printk("page: creating kernel page directory\n");
    /* map the first 4 MB to high half */
    for (i = 0 * 1024 * 1024; i < 4 * 1024 * 1024; i += 4096) {
        page_t pg = create_pte(i, 0, 1);
        kernel_pgt[pte_index(i)] = pg;
    }
    kernel_pgd[pde_index(VIRT_BASE)] = create_pde(kv2p(kernel_pgt), 0, 1);

    /* map 8MB for the heap */
    for (; i < 8 * 1024 * 1024; i += 4096) {
        page_t pg = create_pte(i, 0, 1);
        heap_1_pgt[pte_index(i)] = pg;
    }
    kernel_pgd[pde_index(VIRT_BASE + 4 * 1024 * 1024)]
            = create_pde(kv2p(heap_1_pgt), 0, 1);

    for (; i < 12 * 1024 * 1024; i += 4096) {
        page_t pg = create_pte(i, 0, 1);
        heap_2_pgt[pte_index(i)] = pg;
    }
    kernel_pgd[pde_index(VIRT_BASE + 8 * 1024 * 1024)]
            = create_pde(kv2p(heap_2_pgt), 0, 1);
    kernel_pgd[pde_index(VIRT_BASE + 16 * 10124 * 1024)]
            = create_pde(kv2p(kernel_virt_pgt), 0, 1);

    activate_pgd(kernel_pgd);
    printk("page: kernel directory activated\n");
    ENABLE_IRQ();
}
