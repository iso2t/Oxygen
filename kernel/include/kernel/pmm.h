#ifndef OXYGEN_KERNEL_PMM_H
#define OXYGEN_KERNEL_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PMM_FRAME_SIZE 4096

/* Parse the multiboot2 memory map and build a bitmap of free 4 KiB frames.
 * Marks below-1MiB and the kernel image as reserved. Pass 0 to skip parsing
 * (allocator will report 0 free frames). */
void pmm_init(uintptr_t multiboot_info_addr);

/* Returns the physical address of a 4 KiB frame, or 0 on OOM. */
uintptr_t pmm_alloc_frame(void);

/* Releases a frame previously returned by pmm_alloc_frame(). No-op for 0
 * or addresses outside the tracked region. */
void pmm_free_frame(uintptr_t addr);

/* Frames currently allocatable. */
size_t pmm_free_frames(void);

/* Frames the allocator manages (everything in the mmap that wasn't reserved
 * by pmm_init). Doesn't change after init. */
size_t pmm_total_frames(void);

#endif
