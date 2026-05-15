/*
 * Oxygen kernel - first-fit kernel heap with coalescing.
 *
 * Sits at a fixed virtual range (HEAP_VIRT_BASE..+HEAP_INITIAL_SIZE),
 * backed by physical frames from the PMM and mapped via the VMM at
 * heap_init time. Blocks live in a doubly-linked list; kfree coalesces
 * with adjacent free neighbours so we don't slowly fragment the heap.
 *
 * Not interrupt-safe - add a spinlock when we enable IRQs.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/heap.h"
#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/string.h"
#include "kernel/panic.h"
#include "kernel/kprintf.h"
#include "kernel/spinlock.h"

/* Upper canonical half, PML4[384] - well clear of the kernel's PML4[511].
 * Once we add userspace, kernel page tables share their upper half with
 * user tables, so the heap stays addressable across context switches. */
#define HEAP_VIRT_BASE     0xFFFFC00000000000UL
#define HEAP_INITIAL_SIZE  (1UL * 1024 * 1024)     /* 1 MiB */
#define HEAP_MAGIC         0xC0FFEEBEEFDEAD42ULL
#define HEAP_ALIGN         8

struct block {
    uint64_t      magic;
    size_t        size;        /* user-data size, not including this header */
    int           is_free;
    int           _pad;
    struct block *next;
    struct block *prev;
};

static struct block *heap_head;
static kspinlock_t   heap_lock = KSPINLOCK_INIT;

void heap_init(void) {
    /* Reserve and back the entire heap range up-front. */
    for (size_t off = 0; off < HEAP_INITIAL_SIZE; off += VMM_PAGE_SIZE) {
        uintptr_t phys = pmm_alloc_frame();
        if (!phys) {
            panic("heap_init: PMM OOM at heap offset 0x%zx", off);
        }
        if (vmm_map_4k(HEAP_VIRT_BASE + off, phys, VMM_WRITABLE) != 0) {
            panic("heap_init: vmm_map_4k failed at 0x%lx",
                  (unsigned long)(HEAP_VIRT_BASE + off));
        }
    }

    heap_head = (struct block *)HEAP_VIRT_BASE;
    heap_head->magic   = HEAP_MAGIC;
    heap_head->size    = HEAP_INITIAL_SIZE - sizeof(*heap_head);
    heap_head->is_free = 1;
    heap_head->next    = NULL;
    heap_head->prev    = NULL;
}

void *kmalloc(size_t n) {
    if (n == 0) {
        return NULL;
    }
    n = (n + (HEAP_ALIGN - 1)) & ~(size_t)(HEAP_ALIGN - 1);

    unsigned long _flags = kspinlock_acquire(&heap_lock);
    void *result = NULL;

    for (struct block *b = heap_head; b; b = b->next) {
        if (b->magic != HEAP_MAGIC) {
            panic("kmalloc: heap corruption at %p (magic=0x%lx)",
                  b, (unsigned long)b->magic);
        }
        if (!b->is_free || b->size < n) {
            continue;
        }

        /* Split if the leftover would itself hold a useful allocation. */
        if (b->size >= n + sizeof(struct block) + HEAP_ALIGN) {
            struct block *nb =
                (struct block *)((uint8_t *)b + sizeof(*b) + n);
            nb->magic   = HEAP_MAGIC;
            nb->size    = b->size - n - sizeof(*nb);
            nb->is_free = 1;
            nb->next    = b->next;
            nb->prev    = b;
            if (b->next) {
                b->next->prev = nb;
            }
            b->next = nb;
            b->size = n;
        }

        b->is_free = 0;
        result = (void *)((uint8_t *)b + sizeof(*b));
        break;
    }

    kspinlock_release(&heap_lock, _flags);
    return result;
}

void kfree(void *p) {
    if (!p) {
        return;
    }
    struct block *b = (struct block *)((uint8_t *)p - sizeof(struct block));
    if (b->magic != HEAP_MAGIC) {
        panic("kfree: corrupt header at %p", b);
    }

    unsigned long _flags = kspinlock_acquire(&heap_lock);

    if (b->is_free) {
        kspinlock_release(&heap_lock, _flags);
        panic("kfree: double free at %p", b);
    }
    b->is_free = 1;

    if (b->next && b->next->is_free) {
        b->size += sizeof(struct block) + b->next->size;
        b->next  = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }

    if (b->prev && b->prev->is_free) {
        b->prev->size += sizeof(struct block) + b->size;
        b->prev->next  = b->next;
        if (b->next) {
            b->next->prev = b->prev;
        }
    }

    kspinlock_release(&heap_lock, _flags);
}

void heap_dump(void) {
    size_t used = 0, freebytes = 0, largest = 0;
    size_t n_used = 0, n_free = 0;

    unsigned long _flags = kspinlock_acquire(&heap_lock);
    for (struct block *b = heap_head; b; b = b->next) {
        if (b->is_free) {
            freebytes += b->size;
            n_free++;
            if (b->size > largest) largest = b->size;
        } else {
            used += b->size;
            n_used++;
        }
    }
    kspinlock_release(&heap_lock, _flags);

    /* kprintf has its own lock; calling it outside the heap lock avoids
     * holding two locks at once. */
    kprintf("heap: %zu B used (%zu blocks), %zu B free (%zu blocks, largest %zu B)\n",
            used, n_used, freebytes, n_free, largest);
}
