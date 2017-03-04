#include <levos/arch.h>

#include <stdint.h>

#include "gdt.h"

void
idt_init(void)
{
    return;
}

void
arch_early_init(void)
{
    gdt_init();

    idt_init();

    *(uint16_t *)(0xC03FF000) = 0x1643;
}
