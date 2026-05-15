#ifndef OXYGEN_KERNEL_SPINLOCK_H
#define OXYGEN_KERNEL_SPINLOCK_H

#include <stdint.h>

/*
 * Single-CPU spinlock that also disables interrupts while held.
 *
 * On UP, the only way two execution contexts can race over a lock is
 * if the holder is preempted; the cli inside acquire prevents that.
 * On SMP (eventually) the test-and-set on `locked` will still serialize
 * across CPUs, and the cli is still correct for the local CPU.
 *
 * Usage:
 *     static kspinlock_t L = KSPINLOCK_INIT;
 *     unsigned long flags = kspinlock_acquire(&L);
 *     ... critical section ...
 *     kspinlock_release(&L, flags);
 */

typedef struct {
    volatile int locked;
} kspinlock_t;

#define KSPINLOCK_INIT { 0 }

static inline void kspinlock_init(kspinlock_t *l) {
    l->locked = 0;
}

static inline unsigned long kspinlock_acquire(kspinlock_t *l) {
    unsigned long flags;
    __asm__ volatile ("pushfq; popq %0; cli"
                      : "=r"(flags) :: "memory");
    while (__atomic_test_and_set(&l->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile ("pause" ::: "memory");
    }
    return flags;
}

static inline void kspinlock_release(kspinlock_t *l, unsigned long flags) {
    __atomic_clear(&l->locked, __ATOMIC_RELEASE);
    if (flags & (1UL << 9)) {       /* IF bit was set on entry */
        __asm__ volatile ("sti");
    }
}

#endif
