/*
 * Oxygen kernel - x86_64 boot entry.
 *
 * GRUB enters us in 32-bit protected mode per the Multiboot2 spec.
 * Our job here: zero .bss, build a minimal page table, switch to
 * long mode, and call kmain() with the multiboot info pointer.
 */

.set MULTIBOOT2_MAGIC,     0xe85250d6
.set MULTIBOOT2_ARCH_I386, 0

/* ==========================================================================
 * Multiboot2 header - GRUB looks for this in the first 32 KiB of the file.
 * ========================================================================== */
.section .multiboot_header, "a"
.align 8
mb2_header_start:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCH_I386
    .long mb2_header_end - mb2_header_start
    .long -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH_I386 + (mb2_header_end - mb2_header_start))

    /* end tag */
    .short 0
    .short 0
    .long  8
mb2_header_end:


/* ==========================================================================
 * 32-bit entry. GRUB lands here with the CPU in protected mode and EBX
 * pointing at the multiboot2 information structure.
 * ========================================================================== */
.section .boot, "ax"
.code32
.global _start
.type _start, @function
_start:
    /* Establish our own stack before any pushes / calls. */
    mov $stack_top, %esp

    /* Zero .bss. The ELF loader should already do this, but doing it
     * ourselves removes one assumption. Must happen before we save EBX
     * (which lives in .bss) or touch the page tables. */
    mov $__bss_start, %edi
    mov $__bss_end, %ecx
    sub %edi, %ecx
    shr $2, %ecx
    xor %eax, %eax
    rep stosl

    /* Stash the multiboot info pointer to hand to kmain later. */
    mov %ebx, mb_info_ptr

    /* ------ Build the minimum page tables long mode requires. ------
     * PML4[0] -> PDPT, PDPT[0] -> PD, PD[i] = i * 2 MiB huge page.
     * Net effect: first 1 GiB identity-mapped. Enough to run any kernel
     * that fits in low memory; we'll graduate to a real VM later. */

    mov $pdpt, %eax
    or  $0x3, %eax                  /* present | writable */
    mov %eax, pml4

    mov $pd, %eax
    or  $0x3, %eax
    mov %eax, pdpt

    xor %ecx, %ecx
1:
    mov %ecx, %eax
    shl $21, %eax                   /* eax = index * 2 MiB */
    or  $0x83, %eax                 /* present | writable | page-size */
    mov %eax, pd(,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne 1b

    /* Load CR3 with the PML4 physical address. */
    mov $pml4, %eax
    mov %eax, %cr3

    /* CR4.PAE = 1 (PAE is required to enter long mode). */
    mov %cr4, %eax
    or  $(1 << 5), %eax
    mov %eax, %cr4

    /* EFER.LME = 1 (long-mode enable). */
    mov $0xC0000080, %ecx
    rdmsr
    or  $(1 << 8), %eax
    wrmsr

    /* CR0.PG = 1. Combined with EFER.LME, the CPU is now in long mode
     * (compatibility submode until we far-jump to a 64-bit code segment). */
    mov %cr0, %eax
    or  $(1 << 31), %eax
    mov %eax, %cr0

    /* Load our 64-bit GDT and far-jump into a 64-bit code segment. */
    lgdt gdt64_ptr
    ljmp $GDT64_CODE_SEL, $long_mode_start
.size _start, . - _start


/* ==========================================================================
 * 64-bit entry. From here on we are running real 64-bit code.
 * ========================================================================== */
.code64
.type long_mode_start, @function
long_mode_start:
    /* Segment registers are unused in long mode; clear them for hygiene. */
    xor %ax, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* SysV AMD64 ABI: first arg in RDI. movl to %edi zero-extends. */
    movl mb_info_ptr, %edi

    call kmain

.Lhalt:
    cli
    hlt
    jmp .Lhalt
.size long_mode_start, . - long_mode_start


/* ==========================================================================
 * Static storage.
 * ========================================================================== */
.section .bss, "aw", @nobits
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pd:   .skip 4096

.align 16
mb_info_ptr: .skip 4

.align 16
stack_bottom:
    .skip 16384
.global stack_top
stack_top:


.section .rodata
.align 8
gdt64:
    .quad 0                                         /* null descriptor */
.set GDT64_CODE_SEL, . - gdt64
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)     /* exec | code | present | long */
gdt64_ptr:
    .word . - gdt64 - 1
    .quad gdt64
