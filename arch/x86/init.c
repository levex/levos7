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
enable_sse(void)
{
    //asm volatile ("fninit");
    asm volatile ("clts");
	size_t t;
	asm volatile ("mov %%cr0, %0" : "=r"(t));
	t &= ~(1 << 2);
	t |= (1 << 1);
	asm volatile ("mov %0, %%cr0" :: "r"(t));

	asm volatile ("mov %%cr4, %0" : "=r"(t));
	t |= 3 << 9;
	asm volatile ("mov %0, %%cr4" :: "r"(t));

    /*asm volatile("movl %cr0, %eax;"
                 "andw $0xfffb, %ax;"
                 "orw $0x2, %ax;"
                 "movl %eax, %cr0;"
                 "movl %cr4, %eax;"
                 "orw $(3 << 9), %ax;"
                 "movl %eax, %cr4;"
            );*/
    /*
        mov eax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x2			;set coprocessor monitoring  CR0.MP
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, eax
    ret
    */
}

static uint8_t saves[512] __attribute__((aligned(16)));

void
do_sse_save(struct task *task)
{
    asm volatile ("fxsave (%0)" :: "r"(saves));
	memcpy(task->sse_save, &saves, 512);
}

void
do_sse_restore(struct task *task)
{
	memcpy(&saves, task->sse_save, 512);
    asm volatile ("fxrstor (%0)" :: "r"(saves));
}

void
arch_early_init(uint32_t boot_sig, void *ptr)
{
    tss_init();

    gdt_init();

    idt_init();

    enable_sse();

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

    serial_init();
}

void
arch_very_late_init(void)
{
    ps2_keyboard_init();

    bga_init();
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
