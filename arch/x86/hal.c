#include <levos/arch.h>
#include <levos/kernel.h>

uint8_t
inportb(uint16_t portid)
{
    uint8_t ret;
    asm volatile("inb %%dx, %%al":"=a"(ret):"d"(portid));
    return ret;
}

uint16_t
inportw(uint16_t p)
{
    uint16_t r;
    asm volatile("inw %%dx, %%ax":"=a"(r):"d"(p));
    return r;
}

void
outportb(uint16_t portid, uint8_t value)
{
    asm volatile("outb %%al, %%dx":: "d"(portid), "a"(value));
}

void
outportw(uint16_t portid, uint16_t value)
{
    asm volatile("outw %%ax, %%dx":: "d"(portid), "a"(value));
}
