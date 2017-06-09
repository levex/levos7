#include <levos/kernel.h>
#include <levos/intr.h>
#include <levos/packet.h>
#include <levos/task.h>
#include <stdarg.h>

extern void vprintk(char *, va_list);

extern void *_text_end;
extern void *_text_start;

extern uint32_t *__kernel_map_start;
extern uint32_t *__kernel_map_end;

static int __in_panic = 0;

int
in_panic(void)
{
    return __in_panic;
}

void
print_function_place(uint32_t addr)
{
    char *ptr = (void *) &__kernel_map_start;
    for (; ptr < &__kernel_map_end; ptr += 36) {
        uint32_t cmp = (*(uint32_t *)ptr);
        uint32_t cmp2 = (*(uint32_t *)(ptr + 36));
        // printk("0x%x\n", cmp + 4);
        if (cmp <= addr && addr <= cmp2) {
            char *target = ptr + 4;
            char c;
            c = target[strlen(target) - 1];
            target[strlen(target) - 1] = 0;
            printk("  <0x%x> %s+0x%x/0x%x\n", addr, target, addr - cmp, cmp2 - cmp);
            target[strlen(target)] = c;
            return;
        }
    }

    printk("unknown\n");
    
}

void
dumb_dump_stack(int maxframes)
{
    unsigned int *ebp = &maxframes;
    printk("Stack trace:\n");
    for (int i = 0; i < maxframes; i++) {
        if (*ebp > (uint32_t)&_text_start && *ebp < (uint32_t)&_text_end) {
            printk("  0x%x\n", *ebp);
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
        if (ebp < 4096)
            break;
        unsigned int eip = ebp[1];
        if (eip < (unsigned int)&_text_start || eip > (unsigned int)&_text_end)
            break;
        if (eip < 4096)
            // No caller on stack
            break;
        ebp = (unsigned int *)(ebp[0]);
        unsigned int *arguments = &ebp[2];

        print_function_place(eip);
    }
}

void __noreturn
panic(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    DISABLE_IRQ();
    __in_panic = 1;
    printk("*** Kernel panic: ");
    vprintk(fmt, ap);

    dump_stack(16);
    extern struct task *current_task;
    dump_registers(current_task->regs);
    __dump_code_at(current_task->regs->eip);
    /* dump user stack */
    printk("User stack: \n");
    __hex_dump((int)current_task->regs->esp,
                (int)current_task->regs->esp,
                VIRT_BASE - (unsigned int)current_task->regs->esp,
                0);

    va_end(ap);

    while(1);

    __not_reached();
}
