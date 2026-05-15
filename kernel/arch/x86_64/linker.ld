/*
 * Oxygen kernel linker script - x86_64 higher-half.
 *
 * Physical load address (LMA): 1 MiB (where GRUB drops the kernel).
 * Virtual base for the kernel proper:   0xFFFFFFFF80000000 (upper canonical half).
 * Virtual base for .multiboot/.boot/.bootbss: same as their LMA (low),
 *   because the 32-bit boot code that uses them can't reach high addresses
 *   with 32-bit immediates.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS {
    . = 1M;
    __kernel_phys_start = .;

    /* Multiboot2 header within the first 32 KiB of the file. */
    .multiboot_header : ALIGN(8) {
        KEEP(*(.multiboot_header))
    }

    /* 32-bit boot code + bootstrap GDT, all at low virtual = LMA. */
    .boot : ALIGN(16) {
        *(.boot)
    }

    /* NOBITS storage the 32-bit boot code needs to address: page tables,
     * boot stack, mb_info_ptr. Linked low so 32-bit immediates work. */
    .bootbss : ALIGN(4K) {
        __bootbss_start = .;
        *(.bootbss)
        __bootbss_end   = .;
    }

    /* Everything below lives at high virtual but loads at low physical.
     * The AT() directive sets each section's LMA = VMA - KERNEL_VIRT_BASE,
     * keeping the ELF file's load addresses contiguous from 1 MiB. */
    . += KERNEL_VIRT_BASE;

    .text   : AT(ADDR(.text)   - KERNEL_VIRT_BASE) ALIGN(4K) {
        *(.text) *(.text.*)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) ALIGN(4K) {
        *(.rodata) *(.rodata.*)
    }

    .data   : AT(ADDR(.data)   - KERNEL_VIRT_BASE) ALIGN(4K) {
        *(.data) *(.data.*)
    }

    .bss    : AT(ADDR(.bss)    - KERNEL_VIRT_BASE) ALIGN(4K) {
        __bss_start = .;
        *(COMMON)
        *(.bss) *(.bss.*)
        . = ALIGN(4);
        __bss_end   = .;
    }

    __kernel_end      = .;
    __kernel_phys_end = ADDR(.bss) + SIZEOF(.bss) - KERNEL_VIRT_BASE;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.*)
        *(.comment)
    }
}
