/*
 * Oxygen kernel - top-level interrupt dispatcher.
 *
 * isr.S routes every interrupt (exceptions and IRQs) through a single
 * common stub which calls interrupt_dispatch with the frame. We fan
 * out to either the exception path (vectors 0..31) or the IRQ path
 * (vectors 32..47, post-PIC-remap).
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/idt.h"
#include "kernel/irq.h"
#include "kernel/pic.h"
#include "kernel/kprintf.h"

extern void exception_handler(struct interrupt_frame *f);

static irq_handler_t irq_handlers[16];

void irq_register(int irq, irq_handler_t fn) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = fn;
    }
}

void interrupt_dispatch(struct interrupt_frame *f) {
    if (f->vector < 32) {
        exception_handler(f);
        return;
    }

    if (f->vector < 48) {
        int irq = (int)f->vector - 32;
        if (irq_handlers[irq]) {
            irq_handlers[irq](f);
        }
        /* EOI must follow even unhandled IRQs or the PIC stops delivering. */
        pic_send_eoi((uint8_t)irq);
        return;
    }

    kprintf("interrupt_dispatch: stray vector %lu\n", f->vector);
}
