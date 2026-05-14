#ifndef OXYGEN_KERNEL_KSHELL_H
#define OXYGEN_KERNEL_KSHELL_H

/* Run the kernel shell forever. Signature matches kthread_fn_t so it
 * can be spawned via kthread_create. */
void kshell_run(void *arg);

#endif
