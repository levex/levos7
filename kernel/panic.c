#include <levos/kernel.h>
#include <levos/intr.h>
#include <stdarg.h>

extern void vprintk(char *, va_list);

extern void *_text_end;
extern void *_text_start;

void
dumb_dump_stack(int maxframes)
{
    unsigned int *ebp = &maxframes;
    printk("Stack trace:\n");
    for (int i = 0; i < maxframes; i++) {
        if (*ebp > (uint32_t)&_text_start && *ebp < (uint32_t)&_text_end) {
            printk("  0x%x     \n", *ebp);
        }

        ebp ++;
    }
}

void
dump_stack(int maxframes)
{
    unsigned int *ebp = &maxframes - 2;
    printk("Stack trace:\n");
    for(unsigned int frame = 0; frame < maxframes; ++frame)
    {
        unsigned int eip = ebp[1];
        if (eip < (unsigned int)&_text_start || eip > (unsigned int)&_text_end)
            break;
        if (eip == 0)
            // No caller on stack
            break;
        ebp = (unsigned int *)(ebp[0]);
        unsigned int *arguments = &ebp[2];
        printk("  0x%x\n", eip);
    }
}

void __noreturn
panic(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    DISABLE_IRQ();
    printk("*** Kernel panic: ");
    vprintk(fmt, ap);

    dump_stack(8);

    va_end(ap);

    while(1);

    __not_reached();
}
