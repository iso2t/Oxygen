#include <stdarg.h>

#include "kernel/panic.h"
#include "kernel/kprintf.h"

void panic(const char *fmt, ...) {
    /* Disable interrupts immediately. After this no other thread can run
     * on our CPU, so bypassing the kprintf lock is safe - and necessary,
     * since we may have been called from inside a held lock and would
     * otherwise deadlock. */
    __asm__ volatile ("cli");

    kprintf_unlocked("\n*** KERNEL PANIC: ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf_unlocked(" ***\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
