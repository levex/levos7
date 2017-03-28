#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/intr.h>
#include <levos/task.h>

#include <levos/x86.h>

#define PIT_REG_COUNTER0 0x40
#define PIT_REG_COUNTER1 0x41
#define PIT_REG_COUNTER2 0x42
#define PIT_REG_COMMAND 0x43

#define PIT_OCW_MASK_BINCOUNT 1 //00000001
#define PIT_OCW_MASK_MODE 0xE //00001110
#define PIT_OCW_MASK_RL 0x30 //00110000
#define PIT_OCW_MASK_COUNTER 0xC0 //11000000

#define PIT_OCW_BINCOUNT_BINARY 0 //0
#define PIT_OCW_BINCOUNT_BCD 1 //1

#define PIT_OCW_MODE_TERMINALCOUNT 0 //0000
#define PIT_OCW_MODE_ONESHOT 0x2 //0010
#define PIT_OCW_MODE_RATEGEN 0x4 //0100
#define PIT_OCW_MODE_SQUAREWAVEGEN 0x6 //0110
#define PIT_OCW_MODE_SOFTWARETRIG 0x8 //1000
#define PIT_OCW_MODE_HARDWARETRIG 0xA //1010

#define PIT_OCW_RL_LATCH 0 //000000
#define PIT_OCW_RL_LSBONLY 0x10 //010000
#define PIT_OCW_RL_MSBONLY 0x20 //100000
#define PIT_OCW_RL_DATA 0x30 //110000

#define PIT_OCW_COUNTER_0 0 //00000000
#define PIT_OCW_COUNTER_1 0x40 //01000000
#define PIT_OCW_COUNTER_2 0x80 //10000000

volatile uint32_t __pit_ticks = 0;

void
pit_irq(struct pt_regs *r)
{
    __pit_ticks ++;
}

void
pit_sched_irq(struct pt_regs *r)
{
    __pit_ticks ++;
    sched_tick(r);
}

static inline void __pit_send_cmd(uint8_t cmd)
{
        outportb(PIT_REG_COMMAND, cmd);
}

static inline void __pit_send_data(uint16_t data, uint8_t counter)
{
        uint8_t        port = (counter == PIT_OCW_COUNTER_0) ? PIT_REG_COUNTER0 :
                ((counter == PIT_OCW_COUNTER_1) ? PIT_REG_COUNTER1 : PIT_REG_COUNTER2);

        outportb(port, (uint8_t) data);
}

static inline uint8_t __pit_read_data(uint16_t counter) {

        uint8_t        port = (counter == PIT_OCW_COUNTER_0) ? PIT_REG_COUNTER0 :
                ((counter == PIT_OCW_COUNTER_1) ? PIT_REG_COUNTER1 : PIT_REG_COUNTER2);

        return inportb(port);
}

static void pit_start_counter(uint32_t freq, uint8_t counter, uint8_t mode) {

        if (freq == 0)
                return;

        uint16_t divisor = (uint16_t) (1193181 / (uint16_t) freq);

        // send operational command words
        uint8_t ocw = 0;
        ocw = (ocw & ~PIT_OCW_MASK_MODE)    | mode;
        ocw = (ocw & ~PIT_OCW_MASK_RL)      | PIT_OCW_RL_DATA;
        ocw = (ocw & ~PIT_OCW_MASK_COUNTER) | counter;
        __pit_send_cmd(ocw);

        // set frequency rate
        __pit_send_data(divisor & 0xff, 0);
        __pit_send_data((divisor >> 8) & 0xff, 0);
}

void pit_init(void)
{
    intr_register_hw(32, pit_irq);
    pit_start_counter(200,PIT_OCW_COUNTER_0, PIT_OCW_MODE_SQUAREWAVEGEN);
    printk("x86: pit: clocksource registered\n");
}

void
arch_switch_timer_sched(void)
{
    intr_register_hw(32, pit_sched_irq);
}
