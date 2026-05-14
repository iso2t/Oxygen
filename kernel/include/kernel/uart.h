#ifndef OXYGEN_KERNEL_UART_H
#define OXYGEN_KERNEL_UART_H

#include <stddef.h>

void uart_init(void);
void uart_putc(char c);
void uart_write(const char *buf, size_t n);

#endif
