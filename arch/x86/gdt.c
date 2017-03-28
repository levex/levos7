#include "gdt.h"
#include "tss.h"
#include <levos/kernel.h>

#include <stdint.h>

static uint64_t gdt[SEL_CNT];

uint64_t
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
make_gdtr_operand (uint16_t limit, void *base)
{
    return limit | ((uint64_t) (uint32_t) base << 16);
}

void
gdt_init(void)
{
    uint64_t gdtr_operand;

    /* null descriptor */
    gdt[SEL_NULL  / sizeof(*gdt)] = 0; /* 0x0 */

    /* kernel descriptors */
    gdt[SEL_KCSEG / sizeof(*gdt)] = make_code_desc (0); /* 0x8 */
    gdt[SEL_KDSEG / sizeof(*gdt)] = make_data_desc (0); /* 0x10 */

    /* userspace descriptors */
    gdt[SEL_UCSEG / sizeof(*gdt)] = make_code_desc (3); /* 0x18  / 0x1B*/
    gdt[SEL_UDSEG / sizeof(*gdt)] = make_data_desc (3); /* 0x20  / 0x23*/

    /* TSS */
    gdt[SEL_TSS / sizeof(*gdt)] = make_tss_desc(tss_get());

    gdtr_operand = make_gdtr_operand (sizeof(gdt) - 1, gdt);
    asm volatile ("lgdt %0" : : "m" (gdtr_operand));
    asm volatile ("ltr %w0" : : "q" (SEL_TSS));
}
