#include <levos/kernel.h>
#include <levos/arch.h>
#include <levos/console.h>
#include <levos/intr.h>
#include <levos/syscall.h>
#include <levos/task.h>
#include <levos/multiboot.h>

#include <stdint.h>

#include "gdt.h"
#include "tss.h"
#include "idt.h"

int __x86_total_ram;

void
arch_early_init(uint32_t boot_sig, void *ptr)
{
    tss_init();

    gdt_init();

    idt_init();

    *(uint16_t *)(0xC03FF000) = 0x1643;

    __x86_total_ram = 0;

    if (boot_sig == MULTIBOOT_SIGNATURE)
        multiboot_handle(ptr);

    if (!__x86_total_ram)
        __x86_total_ram = 256 * 1024 * 1024;
}

void
pt_regs_selftest(struct pt_regs *r)
{
    printk("selftest: pt_regs: EAX is 0x%x\n", r->eax);
    if (r->eax == LEVOS_MAGIC)
        printk("selftest: pt_regs: test passed\n");
    else {
        printk("FATAL: pt_regs is broken\n");
        while (1);
    }
}

void
dump_registers(struct pt_regs *regs)
{
   printk("cs=%x ds=%x es=%x fs=%x gs=%x ss=%x\n",
            regs->cs, regs->ds, regs->es, regs->fs, regs->gs, regs->ss);
   printk("eax=%x ebx=%x ecx=%x edx=%x\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
   printk("esi=%x edi=%x\n",
            regs->esi, regs->edi);
   printk("esp=%x eip=%x eflags=%x\n", regs->esp, regs->eip, regs->eflags);
   printk("vector=%x err=%x\n", regs->vec_no, regs->error_code);
   if (current_task)
    printk("pid=%d\n", current_task->pid);
}

void
__prepare_system_call(struct pt_regs *regs)
{
    uint32_t no = regs->eax;
    uint32_t a = regs->ebx;
    uint32_t b = regs->ecx;
    uint32_t c = regs->edx;
    uint32_t d = regs->esi;
    uint32_t stack;

    if (regs->vec_no != 0x80)
        panic("invalid syscall\n");

    asm volatile ("mov %%esp, %0":"=r"(stack));
    //printk("syscall: using stack 0x%x 0x%x\n", stack, regs->esp);

    extern struct task *current_task;
    current_task->sys_regs = regs;
    current_task->regs = regs;
    regs->eax = syscall_hub(no, a, b, c, d);
}

void
arch_preirq_init(void)
{
    pit_init();
}

void
arch_late_init(void)
{
    intr_register_sw(0x60, 0, 0, pt_regs_selftest);
    asm volatile("movl $"__stringify(LEVOS_MAGIC)", %%eax; int $0x60":::"eax");

    intr_register_user(0x80, __prepare_system_call);
}

extern struct console serial_console;
struct console *
arch_get_console(void)
{
    return &serial_console;
}

int
arch_get_total_ram(void)
{
    return __x86_total_ram;
}
