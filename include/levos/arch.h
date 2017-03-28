#ifndef __LEVOS_ARCH_H
#define __LEVOS_ARCH_H

#include <stdint.h>

struct intr_frame;

extern uint32_t *_bss_start;
extern uint32_t *_bss_end;

extern int arch_get_total_ram(void);

#include <levos/x86.h>

void arch_early_init(void);
void arch_preirq_init(void);
void arch_late_init(void);

int arch_get_total_ram(void);

void arch_atomic_or(uint32_t *, uint32_t);
void arch_atomic_and(uint32_t *, uint32_t);
void arch_atomic_xor(uint32_t *, uint32_t);

struct console *arch_get_console(void);

void arch_switch_timer_sched(void);

void arch_spin_lock(int *);
void arch_spin_unlock(int *);

#endif /* __LEVOS_ARCH_H */
