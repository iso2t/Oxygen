#ifndef OXYGEN_KERNEL_IDT_H
#define OXYGEN_KERNEL_IDT_H

#include <stdint.h>

/*
 * Layout of the stack frame an ISR stub assembles before calling the
 * C-level exception handler. Field order matches the order of pushes
 * (stack grows down; last push is first member).
 */
struct interrupt_frame {
    /* pushed by isr_common (in reverse order) */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;

    /* pushed by per-vector stub */
    uint64_t vector;
    uint64_t error_code;     /* hardware-pushed for some vectors; 0 fake otherwise */

    /* pushed by CPU on exception entry */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void idt_init(void);

/* Override the IST index for a single vector after idt_init. `ist` is
 * 0 (use current stack) or 1..7 (use TSS.IST<n>). */
void idt_set_ist(uint8_t vector, uint8_t ist);

/* Called from isr_common in isr.S. */
void exception_handler(struct interrupt_frame *frame);

#endif
