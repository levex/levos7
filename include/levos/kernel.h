#ifndef __LEVOS_KERNEL_H
#define __LEVOS_KERNEL_H

#define LEVOS_VERSION_MAJOR 7
#define LEVOS_VERSION_MINOR 0
#define LEVOS_MAGIC 0x1E405700

/* physical address where the kernel resides */
#define KERNEL_PHYS_BASE 0x00100000
#define KERNEL_VIRT_BASE 0xC0100000

#define VIRT_BASE 0xC0000000

extern void *_kernel_end, *_kernel_start;
#define KERNEL_LENGTH (&_kernel_end - &_kernel_start)
#define KERNEL_PHYS_END (KERNEL_PHYS_BASE + KERNEL_LENGTH)

#include <levos/compiler.h>
#include <levos/string.h>
#include <levos/types.h>
#include <levos/heap.h>
#include <levos/errno.h>

void printk(char *, ...);
void __noreturn panic(char *, ...);

#define panic_on(cond, fmt, ...) if (cond) panic(fmt,##__VA_ARGS__);

#define offsetof(st, m) ((size_t)&(((st *)0)->m))
#define container_of(ptr, type, member) ({ \
            const typeof( ((type *)0)->member ) *__mptr = (ptr); \
            (type *)( (char *)__mptr - offsetof(type,member) );})

#endif /* __LEVOS_KERNEL_H */
