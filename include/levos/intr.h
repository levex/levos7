#ifndef __LEVOS_INTR_H
#define __LEVOS_INTR_H

#include <levos/x86.h>
#include <levos/types.h>

typedef void intr_handler_func (struct pt_regs *);

void intr_register_hw(uint8_t, intr_handler_func *);
void intr_register_sw(uint8_t, int, int, intr_handler_func *);
void intr_register_user(uint8_t, intr_handler_func *);
#endif /* __LEVOS_INTR_H */
