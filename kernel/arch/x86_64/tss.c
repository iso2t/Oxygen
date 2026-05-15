/*
 * Oxygen kernel - GDT + TSS (x86_64).
 *
 * GDT layout (after this commit, post-userspace bring-up):
 *   index 0:   null
 *   index 1:   kernel code     (selector 0x08; matches boot.S so CS stays valid)
 *   index 2:   kernel data     (selector 0x10; needed for SS after syscall)
 *   index 3:   user data       (selector 0x18; usermode uses 0x1B)
 *   index 4:   user code       (selector 0x20; usermode uses 0x23)
 *   index 5-6: TSS descriptor  (16 bytes; selector 0x28)
 *
 * The kdata/udata/ucode ordering is chosen so the syscall/sysret MSR-STAR
 * encoding works: STAR[63:48] = 0x10 means sysret loads CS = (0x10+16)|3 = 0x23
 * (user code) and SS = (0x10+8)|3 = 0x1B (user data). STAR[47:32] = 0x08
 * means syscall enters with CS = 0x08 (kcode) and SS = 0x10 (kdata).
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/tss.h"
#include "kernel/idt.h"
#include "kernel/string.h"

#define GDT_KCODE_SEL  0x08
#define GDT_KDATA_SEL  0x10
#define GDT_UDATA_SEL  0x18
#define GDT_UCODE_SEL  0x20
#define GDT_TSS_SEL    0x28

#define DESC_KCODE  ((1ULL<<43) | (1ULL<<44) | (1ULL<<47) | (1ULL<<53))
#define DESC_KDATA  ((1ULL<<41) | (1ULL<<44) | (1ULL<<47))
#define DESC_UDATA  ((1ULL<<41) | (1ULL<<44) | (1ULL<<45) | (1ULL<<46) | (1ULL<<47))
#define DESC_UCODE  ((1ULL<<43) | (1ULL<<44) | (1ULL<<45) | (1ULL<<46) | (1ULL<<47) | (1ULL<<53))

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

/* 5 8-byte slots for null/kcode/kdata/udata/ucode + 2 for the TSS = 7. */
static uint64_t gdt[7] __attribute__((aligned(16)));

static struct tss64 kernel_tss __attribute__((aligned(16)));

__attribute__((aligned(16)))
static uint8_t df_stack[4096];

static void gdt_install_tss(int idx, uintptr_t base, uint32_t limit) {
    uint64_t low = 0;
    low |= (uint64_t)(limit & 0xFFFFu);
    low |= ((uint64_t)(base & 0xFFFFFFu)) << 16;
    low |= ((uint64_t)0x89) << 40;                       /* available 64-bit TSS */
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

    gdt[0] = 0;
    gdt[1] = DESC_KCODE;
    gdt[2] = DESC_KDATA;
    gdt[3] = DESC_UDATA;
    gdt[4] = DESC_UCODE;
    gdt_install_tss(5, (uintptr_t)&kernel_tss, sizeof(kernel_tss) - 1);

    struct gdt_ptr ptr = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .base  = (uint64_t)(uintptr_t)gdt,
    };
    __asm__ volatile ("lgdt %0" :: "m"(ptr) : "memory");
    __asm__ volatile ("ltr  %w0" :: "r"((uint16_t)GDT_TSS_SEL));

    idt_set_ist(8, IST_DOUBLE_FAULT);
}

void tss_set_rsp0(uintptr_t rsp) {
    kernel_tss.rsp[0] = rsp;
}
