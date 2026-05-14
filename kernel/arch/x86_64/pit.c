/*
 * Oxygen kernel - 8254 Programmable Interval Timer (channel 0 -> IRQ0).
 *
 * Base frequency is 1.193182 MHz; divide it down to the requested rate
 * and program channel 0 in mode 3 (square wave) so it auto-reloads.
 */

#include <stdint.h>

#include "kernel/pit.h"
#include "kernel/io.h"
#include "kernel/irq.h"
#include "kernel/pic.h"
#include "kernel/thread.h"

#define PIT_FREQ_HZ   1193182u
#define PIT_CH0_PORT  0x40
#define PIT_CMD_PORT  0x43

static volatile uint64_t tick_count;

static void pit_isr(struct interrupt_frame *f) {
    (void)f;
    tick_count++;
    schedule();   /* preemption point */
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_FREQ_HZ / hz;
    if (divisor > 0xFFFFu) divisor = 0xFFFFu;
    if (divisor == 0)      divisor = 1;

    /* Channel 0, access mode lo/hi, mode 3 (square wave), binary count. */
    outb(PIT_CMD_PORT, 0x36);
    outb(PIT_CH0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register(0, pit_isr);
    pic_unmask(0);
}

uint64_t pit_ticks(void) {
    return tick_count;
}
