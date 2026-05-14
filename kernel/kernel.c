#include <stdint.h>

#include "kernel/vga.h"
#include "kernel/uart.h"
#include "kernel/kprintf.h"
#include "kernel/idt.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "kernel/string.h"
#include "kernel/pic.h"
#include "kernel/pit.h"
#include "kernel/keyboard.h"

void kmain(uint32_t multiboot_info_addr) {
    vga_clear();
    uart_init();
    idt_init();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("Oxygen - 0.0.1\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Hello, Oxygen.\n");

    pmm_init((uintptr_t)multiboot_info_addr);
    kprintf("pmm:  %zu/%zu frames free (%zu MiB)\n",
            pmm_free_frames(), pmm_total_frames(),
            (pmm_total_frames() * PMM_FRAME_SIZE) / (1024UL * 1024UL));

    vmm_init();
    kprintf("vmm:  kernel page tables loaded\n");

    heap_init();
    kprintf("heap: 1 MiB at %p\n", (void *)0xC000000000UL);

    /* Bring up legacy PICs (remap to 0x20..0x2F) and devices. */
    pic_remap(0x20, 0x28);
    pit_init(PIT_DEFAULT_HZ);
    keyboard_init();

    __asm__ volatile ("sti");
    kprintf("irq:  PIC remapped, PIT @ %d Hz, keyboard armed; sti\n",
            PIT_DEFAULT_HZ);

    /* PIT smoke test: sleep 1 second via hlt+IRQ0 wake-ups. */
    uint64_t start = pit_ticks();
    while (pit_ticks() - start < PIT_DEFAULT_HZ) {
        __asm__ volatile ("hlt");
    }
    kprintf("pit:  100 ticks elapsed (~1s sleep ok)\n");

    /* Keyboard demo: echo until Esc. NOTE: type into the QEMU window;
     * stdin from the WSL terminal goes to COM1 (serial), not PS/2. */
    kprintf("kbd:  type to echo, Esc halts\n> ");
    for (;;) {
        int c = keyboard_getc();
        if (c == 27) {
            kprintf("\n");
            break;
        }
        if (c > 0) {
            kprintf("%c", (char)c);
        }
        __asm__ volatile ("hlt");
    }

    kprintf("kmain: done; halting.\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
