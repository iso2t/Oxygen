#ifndef OXYGEN_KERNEL_VMM_H
#define OXYGEN_KERNEL_VMM_H

#include <stdint.h>
#include <stddef.h>

#define VMM_PAGE_SIZE  4096
#define VMM_HUGE_SIZE  (2UL * 1024 * 1024)

/* Page-table entry flag bits (those we care about right now). */
#define VMM_PRESENT    (1ULL << 0)
#define VMM_WRITABLE   (1ULL << 1)
#define VMM_USER       (1ULL << 2)
#define VMM_NX         (1ULL << 63)

/* Build kernel page tables in C, identity-map the bottom 1 GiB with
 * 2 MiB huge pages, and load CR3. After this, boot.S's bootstrap
 * tables are no longer referenced. */
void vmm_init(void);

/* Map a 4 KiB page at `virt` to physical `phys`. Both must be 4 KiB
 * aligned. `flags` is OR of VMM_WRITABLE / VMM_USER / VMM_NX.
 * Returns 0 on success, -1 on bad alignment or PMM OOM. */
int vmm_map_4k(uintptr_t virt, uintptr_t phys, uint64_t flags);

/* Unmap a 4 KiB virtual page. Returns 0 if it was mapped, -1 otherwise. */
int vmm_unmap_4k(uintptr_t virt);

/* Walk the kernel page tables; return the physical address for `virt`
 * or 0 if it's not mapped (handles huge pages too). */
uintptr_t vmm_translate(uintptr_t virt);

#endif
