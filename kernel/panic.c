#include <levos/kernel.h>
#include <levos/intr.h>
#include <stdarg.h>

extern void vprintk(char *, va_list);

extern void *_text_end;
extern void *_text_start;

void
dump_stack(int maxframes)
{
    unsigned int *ebp = &maxframes;
    printk("Stack trace:\n");
    for (int i = 0; i < maxframes; i++) {
        if (*ebp > &_text_start && *ebp < &_text_end) {
            printk("  0x%x     \n", *ebp);
        }

        ebp ++;
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

    dump_stack(64);

    va_end(ap);

    while(1);

    __not_reached();
}
