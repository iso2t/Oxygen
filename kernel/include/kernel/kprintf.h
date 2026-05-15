#ifndef OXYGEN_KERNEL_KPRINTF_H
#define OXYGEN_KERNEL_KPRINTF_H

#include <stdarg.h>

/* Top-level printf - acquires an internal spinlock so concurrent threads
 * don't shred each other's output. */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Same as kprintf but skips the lock. Use only when interrupts are
 * already disabled and the lock could be held by us (e.g. inside panic). */
void kprintf_unlocked(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Lowest-level - no lock, no varargs setup. */
void kvprintf(const char *fmt, va_list ap);

#endif
