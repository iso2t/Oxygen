#include <stdint.h>
#include "kernel/vga.h"

void kmain(uint32_t multiboot_info_addr) {
    (void)multiboot_info_addr;

    vga_clear();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Oxygen - 0.0.1\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("Hello, Oxygen.\n");
}
