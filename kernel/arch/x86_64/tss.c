/*
 * Oxygen kernel - TSS + GDT (x86_64).
 *
 * In long mode the TSS is mostly vestigial - segmentation is bypassed -
 * but two things still matter:
 *
 *   1. RSP0  - the stack the CPU switches to on a ring-3 -> ring-0
 *              transition (interrupt / syscall from userspace). Not
 *              used yet, but set up so userspace later "just works."
 *   2. IST1..7 - alternate stacks the CPU switches to when an interrupt
 *              vector's IST field is non-zero. The classic use is the
 *              double-fault handler: if the regular stack is corrupt
 *              (overflow, bad RSP), without an IST stack the CPU faults
 *              again pushing the exception frame and triple-faults. With
 *              IST1 pointing at a known-good page, we get a real dump.
 *
 * boot.S left us with a tiny GDT (null + kcode); we replace it here
 * with one that also contains a 64-bit TSS descriptor.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/tss.h"
#include "kernel/idt.h"
#include "kernel/string.h"

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp[3];          /* rsp0..rsp2 */
    uint64_t reserved1;
    uint64_t ist[7];          /* ist[0]=IST1 ... ist[6]=IST7 */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* GDT layout we're building:
 *   index 0:    null         (selector 0x00)
 *   index 1:    kernel code  (selector 0x08, matches boot.S so CS stays valid)
 *   index 2-3:  TSS descriptor, 16 bytes / 2 slots  (selector 0x10)
 */
#define GDT_NULL_SEL   0x00
#define GDT_KCODE_SEL  0x08
#define GDT_TSS_SEL    0x10

static uint64_t gdt[4] __attribute__((aligned(16)));

static struct tss64 kernel_tss __attribute__((aligned(16)));

/* Dedicated stacks the CPU will switch to on IST-using exceptions. */
__attribute__((aligned(16)))
static uint8_t df_stack[4096];

static void gdt_install_tss(int idx, uintptr_t base, uint32_t limit) {
    uint64_t low = 0;
    low |= (uint64_t)(limit & 0xFFFFu);
    low |= ((uint64_t)(base & 0xFFFFFFu)) << 16;
    low |= ((uint64_t)0x89) << 40;                       /* type: available 64-bit TSS */
    low |= ((uint64_t)((limit >> 16) & 0xFu)) << 48;
    low |= ((uint64_t)((base  >> 24) & 0xFFu)) << 56;
    gdt[idx]     = low;
    gdt[idx + 1] = (base >> 32) & 0xFFFFFFFFu;
}

void tss_init(void) {
    memset(&kernel_tss, 0, sizeof(kernel_tss));
    kernel_tss.ist[IST_DOUBLE_FAULT - 1] =
        (uint64_t)(uintptr_t)(df_stack + sizeof(df_stack));
    kernel_tss.iomap_base = sizeof(kernel_tss);          /* no I/O permission map */

    /* Match boot.S's kernel-code descriptor verbatim so CS keeps working
     * across the lgdt. */
    gdt[0] = 0;
    gdt[1] = (1ULL << 43) | (1ULL << 44) | (1ULL << 47) | (1ULL << 53);
    gdt_install_tss(2, (uintptr_t)&kernel_tss, sizeof(kernel_tss) - 1);

    struct gdt_ptr ptr = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .base  = (uint64_t)(uintptr_t)gdt,
    };
    __asm__ volatile ("lgdt %0" :: "m"(ptr) : "memory");
    __asm__ volatile ("ltr  %w0" :: "r"((uint16_t)GDT_TSS_SEL));

    /* Route #DF through IST1 so a corrupt-stack double fault still gets
     * a real exception dump. */
    idt_set_ist(8, IST_DOUBLE_FAULT);
}
