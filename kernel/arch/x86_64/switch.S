/*
 * Oxygen kernel - x86_64 thread context switch + new-thread trampoline.
 *
 * arch_switch saves the callee-saved register set (the only state that
 * matters across a regular C function call) and the return address on
 * the outgoing thread's stack, then loads the incoming thread's RSP
 * and pops the same set off its stack. The ret pops the incoming
 * thread's saved return address - either the resumption point inside
 * schedule() for a previously-running thread, or thread_start_trampoline
 * for a freshly-created thread.
 */

.section .text

/* void arch_switch(uintptr_t *prev_rsp, uintptr_t next_rsp);
 *   rdi -> slot to write outgoing RSP into
 *   rsi  = incoming RSP to load
 */
.global arch_switch
.type   arch_switch, @function
arch_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq  %rsp, (%rdi)        /* save outgoing rsp */
    movq  %rsi, %rsp          /* swap to incoming stack */

    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %rbx
    popq  %rbp

    ret                       /* jumps to incoming thread's saved RIP */
.size arch_switch, . - arch_switch


/* Entry point for newly-created threads, reached via arch_switch's ret.
 * Interrupts are disabled on entry (we came from an IRQ handler that
 * never iretq'd); enable them, then hand off to the C bootstrap which
 * actually invokes the user function.
 */
.global thread_start_trampoline
.type   thread_start_trampoline, @function
thread_start_trampoline:
    sti
    call  thread_bootstrap
    /* thread_bootstrap calls thread_exit which never returns. If for
     * some reason it does, halt forever. */
1:
    cli
    hlt
    jmp   1b
.size thread_start_trampoline, . - thread_start_trampoline
