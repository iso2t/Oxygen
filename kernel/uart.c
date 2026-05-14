#include <stdint.h>
#include <stddef.h>

#include "kernel/uart.h"
#include "kernel/io.h"

#define COM1_BASE  0x3F8

/* Register offsets relative to COM1_BASE. */
#define UART_DATA  0   /* DLAB=0: data; DLAB=1: divisor low  */
#define UART_IER   1   /* DLAB=0: interrupts; DLAB=1: div hi */
#define UART_FCR   2   /* FIFO control (write only)          */
#define UART_LCR   3   /* line control                       */
#define UART_MCR   4   /* modem control                      */
#define UART_LSR   5   /* line status                        */

#define LCR_DLAB   0x80
#define LCR_8N1    0x03
#define LSR_THRE   0x20  /* transmitter holding register empty */

void uart_init(void) {
    /* Disable all UART interrupts; we poll. */
    outb(COM1_BASE + UART_IER, 0x00);

    /* Enable DLAB so we can program the baud divisor. */
    outb(COM1_BASE + UART_LCR, LCR_DLAB);

    /* Divisor latch = 1 -> 115200 baud. */
    outb(COM1_BASE + UART_DATA, 0x01);
    outb(COM1_BASE + UART_IER,  0x00);

    /* 8 data bits, no parity, 1 stop bit. Clears DLAB. */
    outb(COM1_BASE + UART_LCR, LCR_8N1);

    /* Enable+clear both FIFOs, 14-byte trigger level. */
    outb(COM1_BASE + UART_FCR, 0xC7);

    /* DTR + RTS + OUT2 asserted. OUT2 is required for IRQs on real HW
     * even though we don't enable them yet. */
    outb(COM1_BASE + UART_MCR, 0x0B);
}

static inline void uart_tx_one(char c) {
    while ((inb(COM1_BASE + UART_LSR) & LSR_THRE) == 0) {
        /* spin */
    }
    outb(COM1_BASE + UART_DATA, (uint8_t)c);
}

void uart_putc(char c) {
    if (c == '\n') {
        uart_tx_one('\r');
    }
    uart_tx_one(c);
}

void uart_write(const char *buf, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uart_putc(buf[i]);
    }
}
