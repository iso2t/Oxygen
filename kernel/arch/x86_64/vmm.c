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
#include "kernel/spinlock.h"

#define PTE_HUGE       (1ULL << 7)
#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL
#define IDENTITY_BYTES (1024UL * 1024 * 1024)    /* 1 GiB */

static uint64_t   *kernel_pml4;
static kspinlock_t vmm_lock = KSPINLOCK_INIT;

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

/* Fetch (or optionally create) the child table referenced by parent[idx].
 * Newly-created intermediates get VMM_USER set so a ring-3 walk through
 * them succeeds when the leaf entry has VMM_USER too. The leaf's USER bit
 * is still the deciding factor for access; setting USER on intermediates
 * doesn't relax protection on kernel-only leaves. */
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
    parent[idx] = (uintptr_t)t | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
    return t;
}

int vmm_map_4k(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    if ((virt | phys) & 0xFFF) {
        return -1;
    }
    unsigned long _flags = kspinlock_acquire(&vmm_lock);
    int rc = -1;
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 1);
    if (!pdpt) goto out;
    uint64_t *pd = child_table(pdpt, pdpt_idx(virt), 1);
    if (!pd) goto out;
    uint64_t *pt = child_table(pd, pd_idx(virt), 1);
    if (!pt) goto out;

    pt[pt_idx(virt)] = (phys & PTE_ADDR_MASK) | (flags & 0xFFF) | VMM_PRESENT;
    invlpg(virt);
    rc = 0;
out:
    kspinlock_release(&vmm_lock, _flags);
    return rc;
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
    unsigned long _flags = kspinlock_acquire(&vmm_lock);
    int rc = -1;
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 0);
    if (!pdpt) goto out;
    uint64_t *pd = child_table(pdpt, pdpt_idx(virt), 0);
    if (!pd) goto out;
    uint64_t *pt = child_table(pd, pd_idx(virt), 0);
    if (!pt) goto out;
    if (!(pt[pt_idx(virt)] & VMM_PRESENT)) goto out;

    pt[pt_idx(virt)] = 0;
    invlpg(virt);
    rc = 0;
out:
    kspinlock_release(&vmm_lock, _flags);
    return rc;
}

uintptr_t vmm_translate(uintptr_t virt) {
    unsigned long _flags = kspinlock_acquire(&vmm_lock);
    uintptr_t result = 0;
    uint64_t *pdpt = child_table(kernel_pml4, pml4_idx(virt), 0);
    if (!pdpt) goto out;

    uint64_t pdpt_e = pdpt[pdpt_idx(virt)];
    if (!(pdpt_e & VMM_PRESENT)) goto out;
    if (pdpt_e & PTE_HUGE) {
        result = (pdpt_e & PTE_ADDR_MASK) | (virt & 0x3FFFFFFFULL);
        goto out;
    }

    uint64_t *pd = (uint64_t *)(pdpt_e & PTE_ADDR_MASK);
    uint64_t pd_e = pd[pd_idx(virt)];
    if (!(pd_e & VMM_PRESENT)) goto out;
    if (pd_e & PTE_HUGE) {
        result = (pd_e & PTE_ADDR_MASK) | (virt & (VMM_HUGE_SIZE - 1));
        goto out;
    }

    uint64_t *pt = (uint64_t *)(pd_e & PTE_ADDR_MASK);
    uint64_t pt_e = pt[pt_idx(virt)];
    if (!(pt_e & VMM_PRESENT)) goto out;
    result = (pt_e & PTE_ADDR_MASK) | (virt & 0xFFFULL);
out:
    kspinlock_release(&vmm_lock, _flags);
    return result;
}

uint64_t *vmm_kernel_pml4(void) {
    return kernel_pml4;
}

int vmm_map_4k_in(uint64_t *pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
    if ((virt | phys) & 0xFFF) {
        return -1;
    }
    uint64_t *pdpt = child_table(pml4, pml4_idx(virt), 1);
    if (!pdpt) return -1;
    uint64_t *pd = child_table(pdpt, pdpt_idx(virt), 1);
    if (!pd) return -1;
    uint64_t *pt = child_table(pd, pd_idx(virt), 1);
    if (!pt) return -1;

    pt[pt_idx(virt)] = (phys & PTE_ADDR_MASK) | (flags & 0xFFF) | VMM_PRESENT;
    /* No invlpg - this PML4 isn't current; CR3 load will flush TLB. */
    return 0;
}

void vmm_init(void) {
    kernel_pml4 = new_table();
    if (!kernel_pml4) {
        panic("vmm_init: PMM OOM allocating PML4");
    }

    /* Identity-map the bottom 1 GiB with 2 MiB huge pages. Keeps PMM
     * frame access, VGA, and low MMIO addressable via their physical
     * addresses. */
    for (uintptr_t a = 0; a < IDENTITY_BYTES; a += VMM_HUGE_SIZE) {
        if (map_huge_2m(a, a, VMM_WRITABLE) != 0) {
            panic("vmm_init: identity map failed at 0x%lx", a);
        }
    }

    /* Higher-half: map [KERNEL_VIRT_BASE, +1 GiB) to the same first
     * 1 GiB physical. This is what boot.S did during bootstrap; we
     * replicate it in our C-built tables so the running kernel keeps
     * executing across the CR3 reload below. */
    for (uintptr_t off = 0; off < IDENTITY_BYTES; off += VMM_HUGE_SIZE) {
        if (map_huge_2m(KERNEL_VIRT_BASE + off, off, VMM_WRITABLE) != 0) {
            panic("vmm_init: high-half map failed at 0x%lx",
                  KERNEL_VIRT_BASE + off);
        }
    }

    __asm__ volatile ("mov %0, %%cr3" :: "r"(kernel_pml4) : "memory");
}
