/*
 * Oxygen kernel - first userspace process.
 *
 * Allocates a fresh PML4 (cloning the kernel upper half), maps a tiny
 * hand-encoded user program into the lower half, sets TSS.RSP0 and the
 * syscall kernel-stack pointer, then iretq's into ring 3.
 *
 * The embedded user program does:
 *     write(USER_DATA_VIRT, sizeof("Hello from ring 3!\n") - 1);
 *     exit(0);
 *
 * All addresses (user code / data / stack) are fixed for now; per-process
 * layout becomes meaningful once we have more than one user program.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/userspace.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/tss.h"
#include "kernel/string.h"
#include "kernel/kprintf.h"
#include "kernel/panic.h"
#include "kernel/thread.h"
#include "kernel/syscall.h"

#define USER_CODE_VIRT   0x400000UL
#define USER_DATA_VIRT   0x401000UL
#define USER_STACK_VIRT  0x402000UL
#define USER_STACK_SIZE  4096UL
#define USER_STACK_TOP   (USER_STACK_VIRT + USER_STACK_SIZE)

#define GDT_UDATA_SEL    0x18
#define GDT_UCODE_SEL    0x20

/* Hand-encoded x86_64 machine code:
 *
 *     mov  $1, %eax                ; syscall #1 = SYS_WRITE
 *     mov  $0x401000, %rdi         ; buf = USER_DATA_VIRT
 *     mov  $19, %esi               ; len = strlen("Hello from ring 3!\n")
 *     syscall
 *     mov  $0, %eax                ; syscall #0 = SYS_EXIT
 *     syscall
 *     ud2                          ; should not be reached
 */
static const uint8_t user_text[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00,
    0x48, 0xBF, 0x00, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xBE, 0x13, 0x00, 0x00, 0x00,
    0x0F, 0x05,
    0xB8, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x05,
    0x0F, 0x0B,
};

static const char user_msg[] = "Hello from ring 3!\n";

void userspace_run(void *arg) {
    (void)arg;

    kprintf("user: building ring-3 environment\n");

    /* ----- Build a per-process PML4. Upper half = kernel's (shared);
     *       lower half = user mappings we'll fill in below. ----- */
    uintptr_t user_pml4_phys = pmm_alloc_frame();
    if (!user_pml4_phys) {
        panic("user: PMM OOM (PML4)");
    }
    uint64_t *user_pml4   = (uint64_t *)user_pml4_phys;
    uint64_t *kernel_pml4 = vmm_kernel_pml4();
    memset(user_pml4, 0, 4096);
    memcpy(&user_pml4[256], &kernel_pml4[256], 256 * sizeof(uint64_t));

    /* ----- Allocate physical pages for user code, data, stack. ----- */
    uintptr_t code_phys  = pmm_alloc_frame();
    uintptr_t data_phys  = pmm_alloc_frame();
    uintptr_t stack_phys = pmm_alloc_frame();
    if (!code_phys || !data_phys || !stack_phys) {
        panic("user: PMM OOM (pages)");
    }

    /* Map into the user PML4. VMM_USER on every level lets ring 3 walk
     * the tables; the absence of the USER bit on kernel mappings still
     * keeps the upper half opaque to user code. */
    if (vmm_map_4k_in(user_pml4, USER_CODE_VIRT,  code_phys,  VMM_USER) < 0 ||
        vmm_map_4k_in(user_pml4, USER_DATA_VIRT,  data_phys,  VMM_USER) < 0 ||
        vmm_map_4k_in(user_pml4, USER_STACK_VIRT, stack_phys,
                      VMM_WRITABLE | VMM_USER) < 0) {
        panic("user: vmm_map_4k_in failed");
    }

    /* ----- Copy the program text/data into the freshly-mapped frames.
     * We can write through the physical address because the kernel still
     * has the [0, 1 GiB) identity map. ----- */
    memcpy((void *)code_phys, user_text, sizeof(user_text));
    memcpy((void *)data_phys, user_msg,  sizeof(user_msg));
    memset((void *)stack_phys, 0, 4096);

    /* ----- Configure the ring-0 stack the CPU/syscall path will switch
     * to on a ring-3 -> ring-0 transition. TSS.RSP0 is used by interrupts;
     * cpu_local.kernel_rsp is used by syscall. Both point at the top of
     * THIS kthread's kernel stack. ----- */
    uintptr_t kstack_top = thread_kstack_top();
    tss_set_rsp0(kstack_top);
    syscall_set_kernel_rsp(kstack_top);

    kprintf("user: entering ring 3 at 0x%lx (sp=0x%lx)\n",
            (unsigned long)USER_CODE_VIRT, (unsigned long)USER_STACK_TOP);

    /* ----- Swap CR3, swap GS, build an iretq frame, jump to ring 3.
     * The instruction stream survives the CR3 switch because the user
     * PML4's upper half shares our kernel mappings. ----- */
    __asm__ volatile (
        "mov %[cr3], %%cr3\n\t"
        "swapgs\n\t"
        "pushq %[ss]\n\t"
        "pushq %[rsp]\n\t"
        "pushq %[rflags]\n\t"
        "pushq %[cs]\n\t"
        "pushq %[rip]\n\t"
        "iretq\n"
        :
        : [cr3]    "r"(user_pml4_phys),
          [ss]     "i"(GDT_UDATA_SEL | 3),
          [rsp]    "i"(USER_STACK_TOP),
          [rflags] "i"(0x202UL),                /* IF=1, reserved bit */
          [cs]     "i"(GDT_UCODE_SEL | 3),
          [rip]    "i"(USER_CODE_VIRT)
        : "memory"
    );

    __builtin_unreachable();
}
