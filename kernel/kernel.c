#include <stdint.h>

#include "kernel/vga.h"
#include "kernel/uart.h"
#include "kernel/kprintf.h"

void kmain(uint32_t multiboot_info_addr) {
    vga_clear();
    uart_init();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("Oxygen - 0.0.1\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Hello, Oxygen.\n");

    kprintf("multiboot info at %p\n",
            (void *)(uintptr_t)multiboot_info_addr);
    kprintf("kprintf check: dec=%d hex=%08x str=%s\n",
            -42, 0xDEADBEEFu, "ok");
}
