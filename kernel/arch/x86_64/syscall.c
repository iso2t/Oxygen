/*
 * Oxygen kernel - syscall MSR setup + dispatcher.
 *
 * The dispatcher is a plain C function called from syscall_entry.S.
 * Currently supports two calls: SYS_EXIT (0) and SYS_WRITE (1).
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/syscall.h"
#include "kernel/thread.h"
#include "kernel/kprintf.h"

#define MSR_EFER             0xC0000080
#define MSR_STAR             0xC0000081
#define MSR_LSTAR            0xC0000082
#define MSR_FMASK            0xC0000084
#define MSR_GS_BASE          0xC0000101
#define MSR_KERNEL_GS_BASE   0xC0000102

#define EFER_SCE             (1ULL << 0)

/* GDT selectors set up in tss_init: */
#define KCODE_SEL  0x08
#define KDATA_SEL  0x10

/* Per-CPU scratch area reached via %gs in the syscall entry stub.
 * Layout MUST match the %gs:0 / %gs:8 offsets in syscall_entry.S. */
struct cpu_local {
    uint64_t kernel_rsp;        /* gs:0 */
    uint64_t user_rsp_save;     /* gs:8 */
};
static struct cpu_local cpu_local;

extern void syscall_entry(void);

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

void syscall_init(void) {
    cpu_local.kernel_rsp    = 0;
    cpu_local.user_rsp_save = 0;

    /* Enable the syscall/sysret machinery. */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);

    /* STAR encodes the selectors used by syscall and sysret:
     *   [47:32] = kernel CS for syscall   ->  syscall enters CS=KCODE_SEL,
     *                                        SS=KCODE_SEL+8 (= KDATA_SEL)
     *   [63:48] = base for sysret         ->  sysret leaves CS=(base+16)|3,
     *                                        SS=(base+8)|3
     * With base=KDATA_SEL (=0x10), sysret CS = 0x20|3 (user code),
     * sysret SS = 0x18|3 (user data). Matches our GDT layout. */
    wrmsr(MSR_STAR, ((uint64_t)KDATA_SEL << 48) | ((uint64_t)KCODE_SEL << 32));

    /* Where the CPU jumps on `syscall`. */
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);

    /* Bits the CPU clears in RFLAGS on syscall entry: TF, IF, DF. */
    wrmsr(MSR_FMASK, (1ULL << 8) | (1ULL << 9) | (1ULL << 10));

    /* Kernel-side GS points at our cpu_local; the user-side starts at 0
     * and is swapped in via swapgs on the first sysretq / iretq. */
    wrmsr(MSR_GS_BASE,        (uint64_t)(uintptr_t)&cpu_local);
    wrmsr(MSR_KERNEL_GS_BASE, 0);
}

void syscall_set_kernel_rsp(uintptr_t rsp) {
    cpu_local.kernel_rsp = rsp;
}

int64_t syscall_dispatch(uint64_t num, uint64_t a, uint64_t b) {
    switch (num) {
    case SYS_EXIT:
        kprintf("user: exit(%ld)\n", (int64_t)a);
        thread_exit();          /* never returns */
        return -1;

    case SYS_WRITE: {
        /* Naive: user pointer is trusted to be in the lower canonical half.
         * Real kernels copy_from_user with fault handling. */
        const char *buf = (const char *)a;
        if (a >= 0x800000000000ULL) {
            return -1;          /* not a userspace address */
        }
        for (uint64_t i = 0; i < b; i++) {
            kprintf("%c", buf[i]);
        }
        return (int64_t)b;
    }

    default:
        kprintf("syscall: unknown number %lu\n", num);
        return -1;
    }
}
