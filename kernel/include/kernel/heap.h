#ifndef OXYGEN_KERNEL_HEAP_H
#define OXYGEN_KERNEL_HEAP_H

#include <stddef.h>

/* Allocate heap virtual memory range, map physical frames behind it,
 * and seed the free list. Must run after vmm_init(). */
void heap_init(void);

/* Returns at least `size` bytes of zero-or-garbage memory, 8-byte aligned.
 * NULL on OOM (heap is fixed-size for now). */
void *kmalloc(size_t size);

/* Releases a block previously returned by kmalloc(). NULL is a no-op. */
void kfree(void *ptr);

/* Print a one-line summary of heap occupancy. */
void heap_dump(void);

#endif
