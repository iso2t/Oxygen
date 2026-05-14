#ifndef OXYGEN_KERNEL_IO_H
#define OXYGEN_KERNEL_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* Stall ~1 us by bouncing a byte off an unused I/O port. Required after
 * 8259/8254 command writes on old chipsets; harmless on modern ones. */
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" :: "a"((uint8_t)0));
}

#endif
