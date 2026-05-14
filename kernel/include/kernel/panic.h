#ifndef OXYGEN_KERNEL_PANIC_H
#define OXYGEN_KERNEL_PANIC_H

__attribute__((noreturn, format(printf, 1, 2)))
void panic(const char *fmt, ...);

#endif
