#ifndef OXYGEN_KERNEL_IRQ_H
#define OXYGEN_KERNEL_IRQ_H

#include "kernel/idt.h"

typedef void (*irq_handler_t)(struct interrupt_frame *);

/* Register a handler for IRQ line 0..15 (vector = irq + 32). NULL clears. */
void irq_register(int irq, irq_handler_t fn);

#endif
