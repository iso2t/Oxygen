/*
 * Oxygen kernel - virtual memory manager (x86_64 4-level paging).
 *
 * Replaces the bootstrap page tables in boot.S with C-managed ones,
 * exposes a map / unmap / translate API, and supports both 4 KiB and
 * 2 MiB pages (the latter only used internally for the identity map).
 *
 * The kernel page tables live in physical memory the PMM gave us, which
 * the bootstrap tables already identity-mapped, so we can treat their
 * physical addresses as virtual pointers when walking from C.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/string.h"
#include "kernel/panic.h"

#define PTE_HUGE       (1ULL << 7)
#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL
#define IDENTITY_BYTES (1024UL * 1024 * 1024)    /* 1 GiB */

static uint64_t *kernel_pml4;

static inline size_t pml4_idx(uintptr_t v) { return (v >> 39) & 0x1FF; }
static inline size_t pdpt_idx(uintptr_t v) { return (v >> 30) & 0x1FF; }
static inline size_t pd_idx  (uintptr_t v) { return (v >> 21) & 0x1FF; }
static inline size_t pt_idx  (uintptr_t v) { return (v >> 12) & 0x1FF; }

static inline void invlpg(uintptr_t v) {
    __asm__ volatile ("invlpg (%0)" :: "r"(v) : "memory");
}

static uint64_t *new_table(void) {
    uintptr_t f = pmm_alloc_frame();
    if (!f) {
        return NULL;
    }
    memset((void *)f, 0, VMM_PAGE_SIZE);
    return (uint64_t *)f;
}

/* Fetch (or optionally create) the child table referenced by parent[idx]. */
static uint64_t *child_table(uint64_t *parent, size_t idx, int create) {
    if (parent[idx] & VMM_PRESENT) {
        return (uint64_t *)(parent[idx] & PTE_ADDR_MASK);
    }
    if (!create) {
        return NULL;
    }
    uint64_t *t = new_table();
    if (!t) {
        return NULL;
    }
    parent[idx] = (uintptr_t)t | VMM_PRESENT | VMM_WRITABLE;
    return t;
}

int vmm_map_4k(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    if ((virt | phys) & 0xFFF) {
        return -1;
    }
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 1);
    if (!pdpt) return -1;
    uint64_t *pd = child_table(pdpt, pdpt_idx(virt), 1);
    if (!pd) return -1;
    uint64_t *pt = child_table(pd, pd_idx(virt), 1);
    if (!pt) return -1;

    pt[pt_idx(virt)] = (phys & PTE_ADDR_MASK) | (flags & 0xFFF) | VMM_PRESENT;
    invlpg(virt);
    return 0;
}

static int map_huge_2m(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    if ((virt | phys) & (VMM_HUGE_SIZE - 1)) {
        return -1;
    }
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 1);
    if (!pdpt) return -1;
    uint64_t *pd = child_table(pdpt, pdpt_idx(virt), 1);
    if (!pd) return -1;

    pd[pd_idx(virt)] = (phys & ~(VMM_HUGE_SIZE - 1)) |
                       (flags & 0xFFF) | VMM_PRESENT | PTE_HUGE;
    invlpg(virt);
    return 0;
}

int vmm_unmap_4k(uintptr_t virt) {
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 0);
    if (!pdpt) return -1;
    uint64_t *pd = child_table(pdpt, pdpt_idx(virt), 0);
    if (!pd) return -1;
    uint64_t *pt = child_table(pd, pd_idx(virt), 0);
    if (!pt) return -1;
    if (!(pt[pt_idx(virt)] & VMM_PRESENT)) return -1;

    pt[pt_idx(virt)] = 0;
    invlpg(virt);
    return 0;
}

uintptr_t vmm_translate(uintptr_t virt) {
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 0);
    if (!pdpt) return 0;

    uint64_t pdpt_e = pdpt[pdpt_idx(virt)];
    if (!(pdpt_e & VMM_PRESENT)) return 0;
    if (pdpt_e & PTE_HUGE) {           /* 1 GiB page */
        return (pdpt_e & PTE_ADDR_MASK) | (virt & 0x3FFFFFFFULL);
    }

    uint64_t *pd = (uint64_t *)(pdpt_e & PTE_ADDR_MASK);
    uint64_t pd_e = pd[pd_idx(virt)];
    if (!(pd_e & VMM_PRESENT)) return 0;
    if (pd_e & PTE_HUGE) {             /* 2 MiB page */
        return (pd_e & PTE_ADDR_MASK) | (virt & (VMM_HUGE_SIZE - 1));
    }

    uint64_t *pt = (uint64_t *)(pd_e & PTE_ADDR_MASK);
    uint64_t pt_e = pt[pt_idx(virt)];
    if (!(pt_e & VMM_PRESENT)) return 0;
    return (pt_e & PTE_ADDR_MASK) | (virt & 0xFFFULL);
}

void vmm_init(void) {
    kernel_pml4 = new_table();
    if (!kernel_pml4) {
        panic("vmm_init: PMM OOM allocating PML4");
    }

    /* Identity-map the bottom 1 GiB with 2 MiB huge pages, mirroring
     * what boot.S did. After we load CR3 below, everything currently
     * executing (kernel text, data, stack, VGA buffer) stays addressable. */
    for (uintptr_t a = 0; a < IDENTITY_BYTES; a += VMM_HUGE_SIZE) {
        if (map_huge_2m(a, a, VMM_WRITABLE) != 0) {
            panic("vmm_init: failed to identity-map 0x%lx", a);
        }
    }

    __asm__ volatile ("mov %0, %%cr3" :: "r"(kernel_pml4) : "memory");
}
