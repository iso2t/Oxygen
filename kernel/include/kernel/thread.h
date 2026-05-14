#ifndef OXYGEN_KERNEL_THREAD_H
#define OXYGEN_KERNEL_THREAD_H

#include <stdint.h>

typedef void (*kthread_fn_t)(void *arg);

/* Register the currently-running context (kmain) as the bootstrap
 * thread. Must run before kthread_create() / schedule(). */
void sched_init(void);

/* Spawn a new kernel thread. Returns its tid (>= 1) or -1 on OOM. */
int kthread_create(kthread_fn_t fn, void *arg, const char *name);

/* Reconsider which thread should run. Called from the PIT IRQ for
 * preemption; safe to call manually from a thread (it disables and
 * restores interrupts internally). */
void schedule(void);

/* Voluntarily terminate the calling thread. Does not return. */
__attribute__((noreturn)) void thread_exit(void);

/* Print one line per known thread (tid, state, name). */
void thread_dump(void);

#endif
