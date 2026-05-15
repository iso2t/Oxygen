/*
 * Oxygen kernel - x86_64 boot entry (higher-half).
 *
 *   GRUB enters us in 32-bit protected mode at low physical addresses.
 *   We:
 *     - emit a 'B' to COM1 as a tracepoint
 *     - zero .bootbss (low) and build minimal page tables that map
 *       both [0, 1 GiB) identity AND [0xFFFFFFFF80000000, +1 GiB) high
 *     - flip CR4.PAE, EFER.LME, CR0.PG to enter long mode
 *     - lgdt + ljmp to a low 64-bit landing pad
 *     - emit 'L'
 *     - movabs + indirect jmp into the high-virtual `high_entry`
 *     - emit 'H'; switch RSP to the high-virtual stack; zero high .bss;
 *       hand the multiboot info pointer to kmain
 *
 *   If the boot transition fails partway, you'll see only the
 *   tracepoints that printed before the hang.
 */

.set MULTIBOOT2_MAGIC,     0xe85250d6
.set MULTIBOOT2_ARCH_I386, 0
.set COM1_PORT,            0x3F8

/* ==========================================================================
 * Multiboot2 header.
 * ========================================================================== */
.section .multiboot_header, "a"
.align 8
mb2_header_start:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCH_I386
    .long mb2_header_end - mb2_header_start
    .long -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH_I386 + (mb2_header_end - mb2_header_start))

    .short 0
    .short 0
    .long  8
mb2_header_end:


/* ==========================================================================
 * 32-bit entry. Runs at low virtual = low physical.
 * ========================================================================== */
.section .boot, "ax"
.code32
.global _start
.type _start, @function
_start:
    /* Boot stack lives low so 32-bit code can address it. */
    mov $boot_stack_top, %esp

    /* Tracepoint 'B' to COM1. */
    mov $COM1_PORT, %dx
    mov $'B', %al
    outb %al, %dx

    /* Zero .bootbss (page tables + boot stack + mb_info_ptr storage). */
    mov $__bootbss_start, %edi
    mov $__bootbss_end,   %ecx
    sub %edi, %ecx
    shr $2,   %ecx
    xor %eax, %eax
    rep stosl

    /* Stash multiboot info pointer for the high-half entry. */
    mov %ebx, mb_info_ptr

    /* ------ Page tables: dual identity (low) + higher-half (high). ------
     * PML4[0]          -> pdpt_low
     * PML4[511]        -> pdpt_high
     * pdpt_low[0]      -> pd
     * pdpt_high[510]   -> pd        (shared - same 1 GiB physical)
     * pd[i]            =  i * 2 MiB | P | RW | PS    (i = 0..511)
     *
     * Result: virtuals [0, 1 GiB) and [0xFFFFFFFF80000000, +1 GiB) both
     * point at the same physical pages.
     */

    mov $pdpt_low, %eax
    or  $0x3, %eax
    mov %eax, pml4

    mov $pdpt_high, %eax
    or  $0x3, %eax
    mov %eax, (pml4 + 511*8)

    mov $pd, %eax
    or  $0x3, %eax
    mov %eax, pdpt_low
    mov %eax, (pdpt_high + 510*8)

    xor %ecx, %ecx
1:
    mov %ecx, %eax
    shl $21,  %eax              /* index * 2 MiB */
    or  $0x83, %eax             /* P | RW | PS */
    mov %eax, pd(,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne 1b

    /* CR3 = &pml4 */
    mov $pml4, %eax
    mov %eax, %cr3

    /* CR4.PAE = 1 */
    mov %cr4, %eax
    or  $(1 << 5), %eax
    mov %eax, %cr4

    /* EFER.LME = 1 */
    mov $0xC0000080, %ecx
    rdmsr
    or  $(1 << 8), %eax
    wrmsr

    /* CR0.PG = 1 -> long mode (compat submode until far jump). */
    mov %cr0, %eax
    or  $(1 << 31), %eax
    mov %eax, %cr0

    lgdt gdt64_ptr
    ljmp $GDT64_CODE_SEL, $long_mode_low
.size _start, . - _start


/* ==========================================================================
 * 64-bit landing pad - still in low physical .boot section, because the
 * ljmp above can only encode a 32-bit offset.
 * ========================================================================== */
.code64
.type long_mode_low, @function
long_mode_low:
    /* Tracepoint 'L'. */
    mov $COM1_PORT, %dx
    mov $'L', %al
    outb %al, %dx

    /* movabs the 64-bit high-virtual address of the kernel entry, then
     * jmp absolute through it. */
    movabsq $high_entry, %rax
    jmp *%rax
.size long_mode_low, . - long_mode_low


/* ==========================================================================
 * Boot-time GDT. Lives in .boot (low) so 32-bit lgdt can reach it.
 * tss_init() will later install a richer GDT in C.
 * ========================================================================== */
.align 8
gdt64:
    .quad 0
.set GDT64_CODE_SEL, . - gdt64
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)
gdt64_ptr:
    .word . - gdt64 - 1
    .quad gdt64


/* ==========================================================================
 * High-half kernel entry. Linked into .text at 0xFFFFFFFF80...
 * ========================================================================== */
.section .text, "ax"
.code64
.global high_entry
.type   high_entry, @function
high_entry:
    /* Tracepoint 'H'. */
    mov $COM1_PORT, %dx
    mov $'H', %al
    outb %al, %dx

    /* Clear data segment regs (null selectors are fine in long mode). */
    xor %ax, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Switch to the high-virtual kernel stack. movabs because the symbol
     * is in the upper half and out of reach of a 32-bit displacement. */
    movabsq $stack_top, %rsp

    /* Zero high-virtual .bss (kernel C globals). */
    movabsq $__bss_start, %rdi
    movabsq $__bss_end,   %rcx
    sub %rdi, %rcx
    shr $3,   %rcx
    xor %rax, %rax
    rep stosq

    /* Fetch the multiboot info pointer from its low storage and pass it
     * as kmain's first arg. Identity map keeps mb_info_ptr accessible
     * via its low virtual address even from up here. */
    movabsq $mb_info_ptr, %rax
    mov (%rax), %edi              /* zero-extends into rdi */

    call kmain

.Lhang:
    cli
    hlt
    jmp .Lhang
.size high_entry, . - high_entry


/* ==========================================================================
 * Bootstrap NOBITS storage at low addresses.
 * ========================================================================== */
.section .bootbss, "aw", @nobits
.align 4096
pml4:      .skip 4096
pdpt_low:  .skip 4096
pdpt_high: .skip 4096
pd:        .skip 4096

.align 16
mb_info_ptr: .skip 4

.align 16
boot_stack_bottom:
    .skip 4096
boot_stack_top:


/* ==========================================================================
 * Main kernel stack. Lives in high-virtual .bss so the rest of the kernel
 * runs on it after `high_entry` switches RSP.
 * ========================================================================== */
.section .bss, "aw", @nobits
.align 16
stack_bottom:
    .skip 16384
.global stack_top
stack_top:
