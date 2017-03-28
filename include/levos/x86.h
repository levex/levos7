#ifndef __LEVOS_ARCH_X86_H
#define __LEVOS_ARCH_X86_H

#include <stdint.h>

#include <../arch/x86/tss.h>

struct pt_regs
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint16_t gs, :16;
    uint16_t fs, :16;
    uint16_t es, :16;
    uint16_t ds, :16;

    uint32_t vec_no;
    uint32_t error_code;

    void *frame_pointer;

    void (*eip) (void);
    uint16_t cs, :16;
    uint32_t eflags;
    void *esp;
    uint16_t ss, :16;
} __packed;

void     outportw(uint16_t portid, uint16_t value);
void     outportb(uint16_t portid, uint8_t value);
uint8_t  inportb(uint16_t portid);
uint16_t inportw(uint16_t portid);

void pic_init(void);
void pic_eoi(int);
void pit_init(void);

#define ENABLE_IRQ() asm volatile("sti")
#define DISABLE_IRQ() asm volatile("cli")

#endif /* __LEVOS_ARCH_X86_H */
