#ifndef __LEVOS_X86_GDT_H
#define __LEVOS_X86_GDT_H

#include <levos/types.h>

#define SEL_NULL        0x00    /* Null selector. */
#define SEL_KCSEG       0x08    /* Kernel code selector. */
#define SEL_KDSEG       0x10    /* Kernel data selector. */
#define SEL_UCSEG       0x1B    /* User code selector. */
#define SEL_UDSEG       0x23    /* User data selector. */
#define SEL_TSS         0x28    /* Task-state segment. */

#define SEL_CNT         6       /* Number of segments. */

enum seg_class
{
    CLS_SYSTEM = 0,             /* System segment. */
    CLS_CODE_DATA = 1           /* Code or data segment. */
};

enum seg_granularity
{
    GRAN_BYTE = 0,              /* Limit has 1-byte granularity. */
    GRAN_PAGE = 1               /* Limit has 4 kB granularity. */
};

void
gdt_init(void);

uint64_t make_seg_desc(uint32_t,
              uint32_t,
              enum seg_class,
              int,
              int,
              enum seg_granularity);

#endif /* __LEVOS_X86_GDT_H */
