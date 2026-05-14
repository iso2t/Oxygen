#ifndef OXYGEN_KERNEL_KPRINTF_H
#define OXYGEN_KERNEL_KPRINTF_H

#include <stdarg.h>

void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);

#endif
