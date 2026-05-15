#ifndef OXYGEN_KERNEL_SYSCALL_H
#define OXYGEN_KERNEL_SYSCALL_H

#include <stdint.h>

#define SYS_EXIT   0
#define SYS_WRITE  1

/* Configure the syscall/sysret MSRs (EFER.SCE, STAR, LSTAR, FMASK) and the
 * kernel-GS base used by the syscall entry stub. Must run after tss_init
 * because STAR's selector layout depends on the new GDT. */
void syscall_init(void);

/* Update the per-CPU kernel stack pointer that the syscall entry stub
 * switches to on entry from ring 3. Called by userspace_run before iretq. */
void syscall_set_kernel_rsp(uintptr_t rsp);

/* Implemented in C; called from syscall_entry.S. */
int64_t syscall_dispatch(uint64_t num, uint64_t a, uint64_t b);

#endif
