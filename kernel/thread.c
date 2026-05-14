/*
 * Oxygen kernel - kernel threads + round-robin scheduler.
 *
 * Preemptive: the PIT IRQ calls schedule() every tick. Switching
 * happens inside the IRQ handler, but the switch itself is done by
 * arch_switch (in switch.S), which saves callee-saved registers and
 * the return address on the outgoing thread's stack and restores
 * them from the incoming thread's stack. That works because every
 * thread, while suspended, is at the same point in the C call chain
 * (pit_isr -> interrupt_dispatch -> isr_common). New threads are
 * given a synthetic stack so arch_switch's restore lands in
 * thread_start_trampoline, which sti's and calls thread_bootstrap.
 *
 * Not SMP-safe; no real spinlocks yet. Single CPU only.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/thread.h"
#include "kernel/heap.h"
#include "kernel/panic.h"
#include "kernel/kprintf.h"

#define THREAD_STACK_SIZE  (16 * 1024)

enum thread_state {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_DEAD,
};

struct thread {
    uintptr_t          rsp;         /* saved RSP across context switches */
    uintptr_t          kstack_base; /* kmalloc'd stack base, for kfree */
    int                tid;
    enum thread_state  state;
    const char        *name;
    kthread_fn_t       fn;
    void              *arg;
    struct thread     *next;        /* circular run queue */
};

static struct thread *current_thread;
static struct thread *run_queue;
static int            next_tid = 1;

/* In arch/x86_64/switch.S. */
extern void arch_switch(uintptr_t *prev_rsp, uintptr_t next_rsp);
extern void thread_start_trampoline(void);

/* Called from the trampoline on a newly-scheduled thread. */
void thread_bootstrap(void) {
    kthread_fn_t fn  = current_thread->fn;
    void        *arg = current_thread->arg;
    fn(arg);
    thread_exit();
}

static void rq_insert(struct thread *t) {
    if (!run_queue) {
        t->next = t;
        run_queue = t;
        return;
    }
    t->next = run_queue->next;
    run_queue->next = t;
}

void sched_init(void) {
    struct thread *t = kmalloc(sizeof(*t));
    if (!t) {
        panic("sched_init: kmalloc failed");
    }
    t->rsp         = 0;            /* filled by arch_switch on first save */
    t->kstack_base = 0;            /* main thread's stack came from boot.S */
    t->tid         = 0;
    t->state       = THREAD_RUNNING;
    t->name        = "main";
    t->fn          = NULL;
    t->arg         = NULL;
    t->next        = t;

    current_thread = t;
    run_queue      = t;
}

int kthread_create(kthread_fn_t fn, void *arg, const char *name) {
    struct thread *t = kmalloc(sizeof(*t));
    if (!t) return -1;

    void *stack = kmalloc(THREAD_STACK_SIZE);
    if (!stack) {
        kfree(t);
        return -1;
    }

    t->kstack_base = (uintptr_t)stack;
    t->tid         = next_tid++;
    t->state       = THREAD_READY;
    t->name        = name;
    t->fn          = fn;
    t->arg         = arg;

    /* Build the initial stack so arch_switch's ret-pop sequence lands
     * in thread_start_trampoline with all callee-saved regs zeroed. */
    uintptr_t sp_top = ((uintptr_t)stack + THREAD_STACK_SIZE)
                       & ~(uintptr_t)15;
    uint64_t *p = (uint64_t *)sp_top;
    *--p = (uint64_t)thread_start_trampoline;
    *--p = 0;   /* rbp */
    *--p = 0;   /* rbx */
    *--p = 0;   /* r12 */
    *--p = 0;   /* r13 */
    *--p = 0;   /* r14 */
    *--p = 0;   /* r15 */
    t->rsp = (uintptr_t)p;

    rq_insert(t);
    return t->tid;
}

static struct thread *pick_next(void) {
    struct thread *t = current_thread;
    do {
        t = t->next;
        if (t->state == THREAD_READY) {
            return t;
        }
    } while (t != current_thread);
    return current_thread;   /* nothing else runnable */
}

void schedule(void) {
    if (!current_thread) {
        return;   /* sched_init hasn't run yet */
    }

    /* Critical section: the run queue and current_thread are read/written
     * by both the IRQ-entry caller and any manual caller. */
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) :: "memory");

    struct thread *next = pick_next();
    if (next == current_thread) {
        goto out;
    }

    if (current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;
    }
    next->state = THREAD_RUNNING;

    struct thread *prev = current_thread;
    current_thread = next;
    arch_switch(&prev->rsp, next->rsp);

out:
    if (flags & (1ULL << 9)) {     /* restore IF if it was set */
        __asm__ volatile ("sti");
    }
}

void thread_exit(void) {
    __asm__ volatile ("cli");
    current_thread->state = THREAD_DEAD;
    schedule();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void thread_dump(void) {
    if (!run_queue) {
        kprintf("threads: (none)\n");
        return;
    }
    kprintf("  tid  state    name\n");
    struct thread *t = run_queue;
    do {
        const char *state = "?";
        switch (t->state) {
        case THREAD_READY:   state = "ready";   break;
        case THREAD_RUNNING: state = "running"; break;
        case THREAD_DEAD:    state = "dead";    break;
        }
        kprintf("  %3d  %-7s  %s%s\n",
                t->tid, state, t->name,
                (t == current_thread) ? "  <- current" : "");
        t = t->next;
    } while (t != run_queue);
}
