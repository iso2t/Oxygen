/*
 * Oxygen kernel - physical frame allocator (bitmap).
 *
 * Tracks the first 1 GiB of physical memory (which boot.S identity-maps);
 * frames outside that range are simply not eligible for allocation today.
 * Bit semantics: 1 = used/reserved, 0 = free.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/pmm.h"
#include "kernel/string.h"
#include "kernel/multiboot2.h"
#include "kernel/kprintf.h"
#include "kernel/spinlock.h"

#define PMM_TRACKED_BYTES (1024UL * 1024UL * 1024UL)               /* 1 GiB  */
#define PMM_MAX_FRAMES    (PMM_TRACKED_BYTES / PMM_FRAME_SIZE)     /* 262144 */
#define PMM_BITMAP_BYTES  (PMM_MAX_FRAMES / 8)                     /* 32 KiB */

static uint8_t     bitmap[PMM_BITMAP_BYTES];
static size_t      used_count;
static size_t      total_frames;   /* frames the allocator manages after init */
static kspinlock_t pmm_lock = KSPINLOCK_INIT;

/* Provided by linker.ld - physical addresses bracketing the kernel image
 * (low, regardless of where the kernel is linked virtually). */
extern uint8_t __kernel_phys_start[];
extern uint8_t __kernel_phys_end[];

static inline int bit_get(size_t f) {
    return (bitmap[f >> 3] >> (f & 7)) & 1;
}

static inline void bit_set(size_t f) {
    if (!bit_get(f)) {
        bitmap[f >> 3] |= (uint8_t)(1u << (f & 7));
        used_count++;
    }
}

static inline void bit_clear(size_t f) {
    if (bit_get(f)) {
        bitmap[f >> 3] &= (uint8_t)~(1u << (f & 7));
        used_count--;
    }
}

static void mark_range_free(uint64_t addr, uint64_t len) {
    /* Conservative: only frames fully inside [addr, addr+len) become free. */
    uint64_t start = (addr + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t end   = (addr + len) / PMM_FRAME_SIZE;
    if (end > PMM_MAX_FRAMES) {
        end = PMM_MAX_FRAMES;
    }
    for (uint64_t f = start; f < end; f++) {
        bit_clear((size_t)f);
    }
}

static void mark_range_used(uint64_t addr, uint64_t len) {
    /* Aggressive: any frame that overlaps [addr, addr+len) gets reserved. */
    uint64_t start = addr / PMM_FRAME_SIZE;
    uint64_t end   = (addr + len + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    if (end > PMM_MAX_FRAMES) {
        end = PMM_MAX_FRAMES;
    }
    for (uint64_t f = start; f < end; f++) {
        bit_set((size_t)f);
    }
}

void pmm_init(uintptr_t multiboot_info_addr) {
    /* Start with every frame reserved; the mmap walk will free what's real. */
    memset(bitmap, 0xFF, sizeof(bitmap));
    used_count = PMM_MAX_FRAMES;

    if (multiboot_info_addr == 0) {
        kprintf("pmm: no multiboot info pointer; allocator disabled\n");
        return;
    }

    const struct mb2_info *info = (const struct mb2_info *)multiboot_info_addr;
    const uint8_t *cur = (const uint8_t *)info + 8;
    const uint8_t *end = (const uint8_t *)info + info->total_size;

    while (cur + sizeof(struct mb2_tag) <= end) {
        const struct mb2_tag *tag = (const struct mb2_tag *)cur;
        if (tag->type == MB2_TAG_END) {
            break;
        }

        if (tag->type == MB2_TAG_MMAP) {
            const struct mb2_tag_mmap *m = (const struct mb2_tag_mmap *)tag;
            const uint8_t *tag_end = cur + tag->size;
            const uint8_t *e_cur   = cur + sizeof(*m);
            while (e_cur + sizeof(struct mb2_mmap_entry) <= tag_end) {
                const struct mb2_mmap_entry *e =
                    (const struct mb2_mmap_entry *)e_cur;
                if (e->type == MB2_MMAP_AVAILABLE) {
                    mark_range_free(e->addr, e->length);
                }
                e_cur += m->entry_size;
            }
        }

        /* Tags are padded to 8-byte alignment. */
        cur += (tag->size + 7u) & ~7u;
    }

    /* Re-reserve unsafe regions: below 1 MiB (BIOS / VGA / EBDA) and the
     * kernel image itself (physical extent from linker symbols). */
    mark_range_used(0, 0x100000);

    uintptr_t ks = (uintptr_t)__kernel_phys_start;
    uintptr_t ke = (uintptr_t)__kernel_phys_end;
    mark_range_used(ks, ke - ks);

    /* Snapshot what's available to the allocator after init. */
    total_frames = PMM_MAX_FRAMES - used_count;
}

uintptr_t pmm_alloc_frame(void) {
    unsigned long _flags = kspinlock_acquire(&pmm_lock);
    uintptr_t result = 0;
    for (size_t byte = 0; byte < sizeof(bitmap); byte++) {
        if (bitmap[byte] == 0xFF) {
            continue;
        }
        for (unsigned bit = 0; bit < 8; bit++) {
            if (!(bitmap[byte] & (1u << bit))) {
                bitmap[byte] |= (uint8_t)(1u << bit);
                used_count++;
                result = (uintptr_t)(byte * 8 + bit) * PMM_FRAME_SIZE;
                goto done;
            }
        }
    }
done:
    kspinlock_release(&pmm_lock, _flags);
    return result;
}

void pmm_free_frame(uintptr_t addr) {
    if (addr == 0 || (addr % PMM_FRAME_SIZE) != 0) {
        return;
    }
    size_t frame = addr / PMM_FRAME_SIZE;
    if (frame >= PMM_MAX_FRAMES) {
        return;
    }
    unsigned long _flags = kspinlock_acquire(&pmm_lock);
    bit_clear(frame);
    kspinlock_release(&pmm_lock, _flags);
}

size_t pmm_free_frames(void) {
    return PMM_MAX_FRAMES - used_count;
}

size_t pmm_total_frames(void) {
    return total_frames;
}
