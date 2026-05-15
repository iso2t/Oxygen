#ifndef OXYGEN_KERNEL_TSS_H
#define OXYGEN_KERNEL_TSS_H

#include <stdint.h>
#include <stddef.h>

/* IST slots are 1-indexed in the hardware (0 = "no IST"); we keep the
 * same numbering here for clarity. */
#define IST_DOUBLE_FAULT  1

/* Build a fresh GDT (null, kernel code, 64-bit TSS descriptor), point
 * the TSS's IST1 at a dedicated stack, lgdt + ltr. After this returns
 * the CPU knows about our TSS and exceptions configured with IST != 0
 * will switch to the corresponding IST stack on entry. */
void tss_init(void);

/* Update TSS.RSP0 - the stack the CPU switches to on a ring-3 -> ring-0
 * interrupt. Call this whenever a thread is about to enter user mode. */
void tss_set_rsp0(uintptr_t rsp);

#endif
