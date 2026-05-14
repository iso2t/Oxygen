#include <stdarg.h>

#include "kernel/panic.h"
#include "kernel/kprintf.h"

void panic(const char *fmt, ...) {
    kprintf("\n*** KERNEL PANIC: ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf(" ***\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
