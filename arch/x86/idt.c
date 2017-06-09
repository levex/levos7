#include <levos/kernel.h>
#include <levos/arch.h>
#include <levos/intr.h>
#include <levos/task.h>

#include "gdt.h"

#define INTR_CNT 256
static uint64_t idt[INTR_CNT];

typedef void intr_stub_func (void);
extern intr_stub_func *intr_stubs[256];

void *intr_data[256];

static intr_handler_func *intr_handlers[INTR_CNT];

struct task *current_task;

void
__dump_code_at(uint8_t *ptr)
{
    int i;
    uint8_t *base = ptr - 10;

    printk("Code at 0x%x: \n", ptr);
    
    if (ptr < 4096) {
        printk("<NULL PTR>\n");
        return;
    }

    for (i = 0; i < 10; i ++)
        printk("%x ", base[i]);

    printk("<%x>", *ptr);

    for (i = 0; i < 10; i ++)
        printk(" %x", ptr[i + 1]);

    printk("\n");

    return;
}

void __noreturn
handle_kernel_prot_fault(struct pt_regs *regs)
{
   printk("\n");
   printk("--[cut here]--\n");
   printk("General protection fault (#GPF) in the kernel\n");
   printk("Dumping registers:\n");
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
   __dump_code_at(regs->eip);
   panic("General protection fault\n");
   __not_reached();
}

void __noreturn
handle_user_prot_fault(struct pt_regs *regs)
{
    printk("User process %d has GPF'd at 0x%x, sending a signal\n",
            current_task->pid, regs->eip);

    dump_registers(regs);

    send_signal(current_task, SIGSEGV);

    task_exit(current_task);
    __not_reached();
}

void __noreturn
gpf(struct pt_regs *regs)
{
   if ((uint32_t) regs->eip > VIRT_BASE) {
       handle_kernel_prot_fault(regs);
       __not_reached();
   }

   handle_user_prot_fault(regs);
}

void
handle_unexpected_irq(struct pt_regs *regs) {
    DISABLE_IRQ();
    printk("Oops: Unexpected interrupt: %d\n", regs->vec_no);
    dump_registers(regs);
    panic("unexpected interrupt\n");
}

void
handle_illop(struct pt_regs *regs)
{
    if (regs->eip > VIRT_BASE)
        handle_unexpected_irq(regs);

    printk("WARNING: pid=%d executing an Illegal instruction at 0x%x\n",
                current_task->pid, regs->eip);
    send_signal(current_task, SIGILL);
}

void
intr_handler(struct pt_regs *regs)
{
    int external = regs->vec_no >= 0x20 && regs->vec_no < 0x30;

    if (regs->vec_no == 13)
        gpf(regs);
    else if (regs->vec_no == 14) {
        handle_pagefault(regs);
        return;
    } else if (regs->vec_no == 6) {
        handle_illop(regs);
        return;
    }

    intr_handler_func *handler = intr_handlers[regs->vec_no];
    if (handler) {
        handler(regs);
    } else {
        handle_unexpected_irq(regs);
    }

	if (external)
		pic_eoi(regs->vec_no);
}

static uint64_t
make_gate (void (*function) (void), int dpl, int type)
{
    uint32_t d0, d1;

    d0 = (((uint32_t) function & 0xffff)
        | (SEL_KCSEG << 16));

    d1 = (((uint32_t) function & 0xffff0000)
        | (1 << 15)
        | ((uint32_t) dpl << 13)
        | (0 << 12)
        | ((uint32_t) type << 8));

    return d0 | ((uint64_t) d1 << 32);
}

static uint64_t
make_intr_gate(void (*function) (void), int dpl)
{
  return make_gate(function, dpl, 14);
}

static inline uint64_t
make_idtr_operand (uint16_t limit, void *base)
{
    return limit | ((uint64_t) (uint32_t) base << 16);
}

static uint64_t
make_trap_gate (void (*function) (void), int dpl)
{
  return make_gate(function, dpl, 15);
}

static void
register_handler(uint8_t vec_no, int dpl, int level,
                  intr_handler_func *handler)
{
    if (level == 1)
        idt[vec_no] = make_trap_gate (intr_stubs[vec_no], dpl);
    else
        idt[vec_no] = make_intr_gate (intr_stubs[vec_no], dpl);

    intr_handlers[vec_no] = handler;
}

void
intr_register_sw(uint8_t vec_no, int dpl, int level, intr_handler_func *h)
{
    register_handler(vec_no, dpl, level, h);
}

void
intr_register_hw(uint8_t vec_no, intr_handler_func *handler)
{
    register_handler (vec_no, 0, 0, handler);
}

void
intr_set_priv(uint8_t vec_no, void *priv)
{
    intr_data[vec_no] = priv;
}

void *
intr_get_priv(uint8_t vec_no)
{
    return intr_data[vec_no];
}

void
intr_register_user(uint8_t vec_no, intr_handler_func *handler)
{
    register_handler (vec_no, 3, 1, handler);
}


int
idt_init(void)
{
    int i;
    uint64_t idtr_operand;

    pic_init();

    for (i = 0; i < INTR_CNT; i++)
        idt[i] = make_intr_gate(intr_stubs[i], 0);

    idtr_operand = make_idtr_operand(sizeof(idt) - 1, idt);
    asm volatile ("lidt %0" : : "m" (idtr_operand));

    return 0;
}
