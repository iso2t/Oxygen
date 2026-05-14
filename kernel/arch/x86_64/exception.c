#include <stdint.h>

#include "kernel/idt.h"
#include "kernel/kprintf.h"

static const char *const exception_names[32] = {
    [0]  = "Divide-by-zero",
    [1]  = "Debug",
    [2]  = "NMI",
    [3]  = "Breakpoint",
    [4]  = "Overflow",
    [5]  = "Bound Range Exceeded",
    [6]  = "Invalid Opcode",
    [7]  = "Device Not Available",
    [8]  = "Double Fault",
    [9]  = "Coprocessor Segment Overrun",
    [10] = "Invalid TSS",
    [11] = "Segment Not Present",
    [12] = "Stack-Segment Fault",
    [13] = "General Protection Fault",
    [14] = "Page Fault",
    [15] = "Reserved",
    [16] = "x87 Floating-Point Exception",
    [17] = "Alignment Check",
    [18] = "Machine Check",
    [19] = "SIMD Floating-Point Exception",
    [20] = "Virtualization Exception",
    [21] = "Control Protection Exception",
};

static const char *exception_name(uint64_t vector) {
    if (vector < 32 && exception_names[vector]) {
        return exception_names[vector];
    }
    return "Reserved";
}

void exception_handler(struct interrupt_frame *f) {
    kprintf("\n");
    kprintf("======================================================================\n");
    kprintf(" EXCEPTION %lu: %s   (err=0x%lx)\n",
            f->vector, exception_name(f->vector), f->error_code);
    kprintf("----------------------------------------------------------------------\n");
    kprintf("  rip=0x%016lx  cs=0x%04lx  rflags=0x%lx\n",
            f->rip, f->cs, f->rflags);
    kprintf("  rsp=0x%016lx  ss=0x%04lx\n", f->rsp, f->ss);
    kprintf("  rax=0x%016lx  rbx=0x%016lx\n", f->rax, f->rbx);
    kprintf("  rcx=0x%016lx  rdx=0x%016lx\n", f->rcx, f->rdx);
    kprintf("  rsi=0x%016lx  rdi=0x%016lx\n", f->rsi, f->rdi);
    kprintf("  rbp=0x%016lx\n", f->rbp);
    kprintf("  r8 =0x%016lx  r9 =0x%016lx\n", f->r8,  f->r9);
    kprintf("  r10=0x%016lx  r11=0x%016lx\n", f->r10, f->r11);
    kprintf("  r12=0x%016lx  r13=0x%016lx\n", f->r12, f->r13);
    kprintf("  r14=0x%016lx  r15=0x%016lx\n", f->r14, f->r15);

    if (f->vector == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        kprintf("  cr2=0x%016lx  (faulting address)\n", cr2);
    }

    kprintf("======================================================================\n");
    kprintf(" halted\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
