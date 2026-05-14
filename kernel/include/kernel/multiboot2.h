#ifndef OXYGEN_KERNEL_MULTIBOOT2_H
#define OXYGEN_KERNEL_MULTIBOOT2_H

#include <stdint.h>

/* GRUB passes this value in EAX at kernel entry (we don't check it yet). */
#define MB2_BOOTLOADER_MAGIC 0x36d76289u

/* Selected tag types we care about right now. */
#define MB2_TAG_END   0
#define MB2_TAG_MMAP  6

/* Memory map entry types. */
#define MB2_MMAP_AVAILABLE        1
#define MB2_MMAP_RESERVED         2
#define MB2_MMAP_ACPI_RECLAIMABLE 3
#define MB2_MMAP_ACPI_NVS         4
#define MB2_MMAP_BAD_RAM          5

struct mb2_info {
    uint32_t total_size;
    uint32_t reserved;
    /* tags follow, each 8-byte aligned */
};

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* mb2_mmap_entry[] follows */
} __attribute__((packed));

#endif
