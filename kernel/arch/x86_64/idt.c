#include <stdint.h>
#include <stddef.h>

#include "kernel/idt.h"

#define IDT_MAX_VECTORS  256
#define KERNEL_CS        0x08   /* matches gdt64 code selector in boot.S */

/* Interrupt-gate, present, DPL=0. */
#define IDT_TYPE_INT_GATE  0x8E

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;          /* bits 0-2: IST index (0 = legacy stack) */
    uint8_t  type_attr;    /* P | DPL | 0 | type */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_MAX_VECTORS];

/* Defined in isr.S - array of 48 stub function pointers (0-31 = CPU
 * exceptions, 32-47 = remapped IRQs). */
extern void *isr_stubs[48];

static void idt_set_gate(uint8_t vector, uint64_t handler,
                         uint8_t ist, uint8_t type_attr) {
    struct idt_entry *e = &idt[vector];
    e->offset_low  = (uint16_t)(handler & 0xFFFF);
    e->selector    = KERNEL_CS;
    e->ist         = ist & 0x7;
    e->type_attr   = type_attr;
    e->offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    e->offset_high = (uint32_t)(handler >> 32);
    e->reserved    = 0;
}

void idt_init(void) {
    for (int v = 0; v < 48; v++) {
        idt_set_gate((uint8_t)v, (uint64_t)isr_stubs[v], 0, IDT_TYPE_INT_GATE);
    }

    struct idt_pointer p = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base  = (uint64_t)(uintptr_t)idt,
    };
    __asm__ volatile ("lidt %0" :: "m"(p));
}
