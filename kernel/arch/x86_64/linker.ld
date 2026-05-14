/*
 * Oxygen kernel linker script - x86_64 ELF, loaded by GRUB at 1 MiB.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

SECTIONS {
    . = 1M;
    __kernel_start = .;

    /* The multiboot2 header must live within the first 32 KiB of the
     * file, so we place it first. */
    .multiboot_header : ALIGN(8) {
        KEEP(*(.multiboot_header))
    }

    /* 32-bit boot code follows immediately. */
    .boot : ALIGN(16) {
        *(.boot)
    }

    .text : ALIGN(4K) {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4K) {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4K) {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        . = ALIGN(4);
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.*)
        *(.comment)
    }
}
