/*
 * Oxygen kernel - PS/2 keyboard (IRQ1).
 *
 * Read raw scan-code-set-1 bytes from port 0x60, translate the pressed
 * subset (top bit clear) into ASCII, drop into a tiny ring buffer.
 * Release events, extended (0xE0) sequences and modifier tracking are
 * deliberately ignored for now.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/keyboard.h"
#include "kernel/io.h"
#include "kernel/irq.h"
#include "kernel/pic.h"

#define KB_DATA_PORT  0x60
#define KB_BUF_SIZE   64

static volatile uint8_t kb_buf[KB_BUF_SIZE];
static volatile size_t  kb_head;
static volatile size_t  kb_tail;

/* Scancode set 1 -> ASCII (lowercase, US QWERTY). 0 = unmapped. */
static const char scancode_to_ascii[128] = {
    /* 0x00 */ 0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
    /* 0x0A */ '9', '0', '-', '=', '\b','\t','q', 'w', 'e', 'r',
    /* 0x14 */ 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    /* 0x1E */ 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    /* 0x28 */ '\'','`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',
    /* 0x32 */ 'm', ',', '.', '/', 0,   '*', 0,   ' ',
    /* rest unmapped */
};

static void keyboard_isr(struct interrupt_frame *f) {
    (void)f;
    uint8_t sc = inb(KB_DATA_PORT);
    if (sc & 0x80) {
        return;                 /* key release */
    }

    char c = scancode_to_ascii[sc & 0x7F];
    if (!c) {
        return;                 /* unmapped key */
    }

    size_t next = (kb_head + 1) % KB_BUF_SIZE;
    if (next == kb_tail) {
        return;                 /* buffer full - drop */
    }
    kb_buf[kb_head] = (uint8_t)c;
    kb_head = next;
}

void keyboard_init(void) {
    kb_head = 0;
    kb_tail = 0;
    irq_register(1, keyboard_isr);
    pic_unmask(1);
}

int keyboard_getc(void) {
    if (kb_head == kb_tail) {
        return -1;
    }
    char c = (char)kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return (int)(unsigned char)c;
}
