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

#define HEAP_VIRT_BASE     0xC000000000UL          /* 768 GiB into VA space */
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
        return (void *)((uint8_t *)b + sizeof(*b));
    }
    return NULL;
}

void kfree(void *p) {
    if (!p) {
        return;
    }
    struct block *b = (struct block *)((uint8_t *)p - sizeof(struct block));
    if (b->magic != HEAP_MAGIC) {
        panic("kfree: corrupt header at %p", b);
    }
    if (b->is_free) {
        panic("kfree: double free at %p", b);
    }
    b->is_free = 1;

    /* Merge forward. */
    if (b->next && b->next->is_free) {
        b->size += sizeof(struct block) + b->next->size;
        b->next = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }

    /* Merge backward. */
    if (b->prev && b->prev->is_free) {
        b->prev->size += sizeof(struct block) + b->size;
        b->prev->next = b->next;
        if (b->next) {
            b->next->prev = b->prev;
        }
    }
}

void heap_dump(void) {
    size_t used = 0, freebytes = 0, largest = 0;
    size_t n_used = 0, n_free = 0;
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
    kprintf("heap: %zu B used (%zu blocks), %zu B free (%zu blocks, largest %zu B)\n",
            used, n_used, freebytes, n_free, largest);
}
