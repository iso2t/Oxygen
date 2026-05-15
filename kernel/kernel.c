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
#include "kernel/thread.h"
#include "kernel/kshell.h"
#include "kernel/tss.h"

void kmain(uint32_t multiboot_info_addr) {
    vga_clear();
    uart_init();
    idt_init();
    tss_init();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("Oxygen - 0.0.1\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    pmm_init((uintptr_t)multiboot_info_addr);
    kprintf("pmm:  %zu/%zu frames free (%zu MiB)\n",
            pmm_free_frames(), pmm_total_frames(),
            (pmm_total_frames() * PMM_FRAME_SIZE) / (1024UL * 1024UL));

    vmm_init();
    kprintf("vmm:  kernel page tables loaded\n");
    kprintf("kernel: kmain @ %p\n", (void *)kmain);

    heap_init();
    kprintf("heap: 1 MiB at %p\n", (void *)heap_init);

    uint16_t tr;
    __asm__ volatile ("str %0" : "=r"(tr));
    kprintf("tss:  TR=0x%x, #DF routed to IST1\n", tr);

    pic_remap(0x20, 0x28);
    pit_init(PIT_DEFAULT_HZ);
    keyboard_init();
    sched_init();

    __asm__ volatile ("sti");
    kprintf("irq:  PIC remapped, PIT @ %d Hz, keyboard armed; sti\n",
            PIT_DEFAULT_HZ);

    if (kthread_create(kshell_run, NULL, "kshell") < 0) {
        kprintf("kmain: failed to spawn kshell\n");
    } else {
        kprintf("sched: kshell thread spawned\n");
    }

    /* kmain becomes the idle thread: halt until something preempts us. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
