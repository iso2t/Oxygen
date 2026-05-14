#ifndef OXYGEN_KERNEL_PIT_H
#define OXYGEN_KERNEL_PIT_H

#include <stdint.h>

#define PIT_DEFAULT_HZ 100

/* Configure channel 0 of the PIT to fire IRQ0 at `hz` Hz, install the
 * tick handler, and unmask IRQ0 at the PIC. Caller is responsible for
 * STI later. */
void pit_init(uint32_t hz);

/* Monotonic tick counter, incremented from the IRQ0 handler. */
uint64_t pit_ticks(void);

#endif
