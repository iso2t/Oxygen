#include "kernel/vga.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_BUFFER  ((volatile uint16_t *)0xB8000)

static size_t  cursor_row;
static size_t  cursor_col;
static uint8_t cursor_color = 0x07; /* light grey on black */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    cursor_color = (uint8_t)fg | ((uint8_t)bg << 4);
}

void vga_clear(void) {
    const uint16_t blank = vga_entry(' ', cursor_color);
    for (size_t i = 0; i < (size_t)VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    const uint16_t blank = vga_entry(' ', cursor_color);
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
}

static void vga_newline(void) {
    cursor_col = 0;
    if (++cursor_row == VGA_HEIGHT) {
        vga_scroll();
        cursor_row = VGA_HEIGHT - 1;
    }
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_newline();
        return;
    }
    VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, cursor_color);
    if (++cursor_col == VGA_WIDTH) {
        vga_newline();
    }
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}
