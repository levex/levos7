#include <levos/kernel.h>
#include <levos/x86.h>
#include <levos/task.h>
#include "gdt.h"

/* Kernel TSS. */
static struct tss tss __attribute__((aligned(4096)));

static char irq_stack[4096];

/* Initializes the kernel TSS. */
void
tss_init (void) 
{
    memset(&tss, 0, sizeof(struct tss));
    tss.ss0 = 0x10;
    tss.bitmap = 0xdfff;
    memset(irq_stack, 0, sizeof(irq_stack));
}

/* Returns the kernel TSS. */
struct tss *
tss_get (void) 
{
    return &tss;
}

/* Sets the ring 0 stack pointer in the TSS to point to the end
   of the thread stack. */
void
tss_update (struct task *task) 
{
    tss.esp0 = task->irq_stack_top;
}

uint64_t
make_tss_desc (void *laddr)
{
    return
      make_seg_desc((uint32_t) laddr,
              0x67, CLS_SYSTEM, 9, 0, GRAN_BYTE);
}
