#include "gdt.h"

#include <stdint.h>

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

#define SEL_CNT 6
static uint64_t gdt[SEL_CNT];

static uint64_t
make_seg_desc(uint32_t base,
              uint32_t limit,
              enum seg_class class,
              int type,
              int dpl,
              enum seg_granularity granularity)
{
    uint32_t d0, d1;

    d0 = ((limit & 0xffff)             /* Limit 15:0. */
          | (base << 16));             /* Base 15:0. */

    d1 = (((base >> 16) & 0xff)        /* Base 23:16. */
          | (type << 8)                /* Segment type. */
          | (class << 12)              /* 0=system, 1=code/data. */
          | (dpl << 13)                /* Descriptor privilege. */
          | (1 << 15)                  /* Present. */
          | (limit & 0xf0000)          /* Limit 16:19. */
          | (1 << 22)                  /* 32-bit segment. */
          | (granularity << 23)        /* Byte/page granularity. */
          | (base & 0xff000000));      /* Base 31:24. */

    return d0 | ((uint64_t) d1 << 32);
}

static uint64_t
make_code_desc (int dpl)
{
    return make_seg_desc (0, 0xfffff, CLS_CODE_DATA, 10, dpl, GRAN_PAGE);
}
static uint64_t
make_data_desc (int dpl)
{
    return make_seg_desc (0, 0xfffff, CLS_CODE_DATA, 2, dpl, GRAN_PAGE);
}

static uint64_t
make_tss_desc (void *laddr)
{
    return make_seg_desc ((uint32_t) laddr, 0x67, CLS_SYSTEM, 9, 0, GRAN_BYTE);
}

static uint64_t
make_gdtr_operand (uint16_t limit, void *base)
{
    return limit | ((uint64_t) (uint32_t) base << 16);
}

void
gdt_init(void)
{
    uint64_t gdtr_operand;

    /* null descriptor */
    gdt[SEL_NULL  / sizeof(*gdt)] = 0;

    /* kernel descriptors */
    gdt[SEL_KCSEG / sizeof(*gdt)] = make_code_desc (0);
    gdt[SEL_KDSEG / sizeof(*gdt)] = make_data_desc (0);

    /* userspace descriptors */
    gdt[SEL_UCSEG / sizeof(*gdt)] = make_code_desc (3);
    gdt[SEL_UDSEG / sizeof(*gdt)] = make_data_desc (3);

    gdtr_operand = make_gdtr_operand (sizeof(gdt) - 1, gdt);
    asm volatile ("lgdt %0" : : "m" (gdtr_operand));
}
