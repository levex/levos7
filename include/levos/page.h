#ifndef __LEVOS_PAGE_H
#define __LEVOS_PAGE_H

#include <levos/arch.h>

typedef uint32_t     page_t;
typedef uint32_t     pde_t;

typedef page_t      *pagetable_t;
typedef pde_t       *pagedir_t;

#define PTE_PRESENT_SHIFT   0
#define PTE_RW_SHIFT        1
#define PTE_USER_SHIFT      2
#define PTE_WRITETHRU_SHIFT 3
#define PTE_CACHE_SHIFT     4
#define PTE_ACCESS_SHIFT    5
#define PTE_DIRTY_SHIFT     6
#define PTE_ZERO_SHIFT      7
#define PTE_ZERO_MASK       (~(1 << PTE_ZERO_SHIFT))
#define PTE_GLOB_SHIFT      8
#define PTE_COW_SHIFT       9
#define PTE_AVAIL_MASK      (~((1 << 9) | (1 << 10) | (1 << 11)))
#define PTE_ADDR_SHIFT      12

#define PDE_PRESENT_SHIFT   0
#define PDE_RW_SHIFT        1
#define PDE_USER_SHIFT      2
#define PDE_WRITETHRU_SHIFT 3
#define PDE_CACHE_SHIFT     4
#define PDE_ACCESS_SHIFT    5
#define PDE_ZERO_SHIFT      6
#define PDE_ZERO_MASK       (~(1 << PTE_ZERO_SHIFT))
#define PDE_SIZE_SHIFT      7
#define PDE_IGNORE_SHIFT    8
#define PDE_COW_SHIFT       9
#define PDE_AVAIL_MASK      (~((1 << 9) | (1 << 10) | (1 << 11)))
#define PDE_ADDR_SHIFT      12

#define PG_P_USER     1
#define PG_P_KERN     0

#define PG_R_RO       0
#define PG_R_RW       1

#define PG_RND_DOWN(a) ROUND_DOWN(a, 0x1000)

void paging_init(void);
int map_page(pagedir_t, uint32_t, uint32_t, int);
int map_page_curr(uint32_t, uint32_t, int);
pagedir_t new_page_directory(void);
pagedir_t copy_page_dir(pagedir_t);
void replace_page(pagedir_t, uint32_t, pde_t);

void handle_pagefault(struct pt_regs *);


int page_mapped(pagedir_t, uint32_t);
int page_mapped_curr(uint32_t);


extern pde_t kernel_pgd[1024] __page_align;
void __flush_tlb(void);

inline void activate_pgd(pagedir_t pgd)
{
    asm volatile("mov %0, %%cr3"::"r"((int)pgd - (int)VIRT_BASE));
}

inline pagedir_t __save_pgd(void)
{
    uint32_t ret;
    asm volatile("mov %%cr3, %0":"=r"(ret));
    return (pagedir_t) (ret + VIRT_BASE);
}

inline pagedir_t activate_pgd_save(pagedir_t pgd)
{
    pagedir_t ret = __save_pgd();
    asm volatile("mov %0, %%cr3"::"r"((int)pgd - (int)VIRT_BASE));
    return ret;
}

void virt_kmap_init(void);
void *kmap_get_free_address(void);
void *kmap_get_page(void);

#endif /* __LEVOS_PAGE_H */
