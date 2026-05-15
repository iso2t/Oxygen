#ifndef OXYGEN_KERNEL_USERSPACE_H
#define OXYGEN_KERNEL_USERSPACE_H

/* Run a tiny embedded user program in ring 3. Signature matches
 * kthread_fn_t so it can be spawned via kthread_create. Does not return -
 * the user program calls sys_exit which terminates the calling kthread. */
void userspace_run(void *arg);

#endif
