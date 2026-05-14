/*
 * Oxygen kernel - 8259 PIC pair (master at 0x20/0x21, slave at 0xA0/0xA1).
 *
 * The legacy PIC fires its IRQ0..15 on vectors 8..15 / 0x70..0x77 by
 * default - which collides head-on with the architectural exception
 * vectors. pic_remap() shifts the lines to whatever range we choose
 * (we pick 0x20..0x2F, immediately above the 32 exception vectors).
 */

#include <stdint.h>

#include "kernel/pic.h"
#include "kernel/io.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

#define PIC_EOI    0x20

void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    /* Start the initialization sequence on both PICs. */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();

    /* ICW2: vector offsets. */
    outb(PIC1_DATA, master_offset); io_wait();
    outb(PIC2_DATA, slave_offset);  io_wait();

    /* ICW3: master tells slave it's on IRQ2; slave tells its identity. */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    /* ICW4: 8086 mode. */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask everything; drivers unmask the lines they own. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, (uint8_t)(inb(port) | (1u << irq)));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, (uint8_t)(inb(port) & ~(1u << irq)));
}
