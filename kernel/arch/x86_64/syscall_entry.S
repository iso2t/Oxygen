/*
 * Oxygen kernel - syscall entry stub.
 *
 * Reached via the `syscall` instruction from ring 3. The CPU has loaded
 * RCX = saved RIP, R11 = saved RFLAGS (with FMASK bits cleared), CS/SS
 * from STAR, and left RSP pointing at the user stack.
 *
 * We swapgs to gain access to per-CPU kernel data (cpu_local), stash
 * the user RSP there, load the kernel RSP, translate the args from the
 * syscall ABI to the SysV C ABI, and call syscall_dispatch. On return
 * we reverse it and sysretq back to user mode.
 */

.section .text
.global syscall_entry
.type   syscall_entry, @function
syscall_entry:
    swapgs                       /* GS_BASE <-> KERNEL_GS_BASE */
    mov   %rsp, %gs:8            /* save user RSP at cpu_local.user_rsp_save */
    mov   %gs:0, %rsp            /* load kernel RSP from cpu_local.kernel_rsp */

    pushq %rcx                   /* user RIP - needed for sysretq */
    pushq %r11                   /* user RFLAGS - needed for sysretq */

    /* Translate syscall ABI -> SysV ABI:
     *   syscall: rax=num, rdi=arg0, rsi=arg1
     *   sysv:    rdi=num, rsi=arg0, rdx=arg1 (for syscall_dispatch's signature) */
    mov   %rsi, %rdx
    mov   %rdi, %rsi
    mov   %rax, %rdi

    sti                          /* safe to take interrupts now */
    call  syscall_dispatch
    cli                          /* close the window before sysretq */

    popq  %r11
    popq  %rcx

    mov   %gs:8, %rsp            /* restore user RSP */
    swapgs                       /* GS_BASE <-> KERNEL_GS_BASE (back to user) */
    sysretq
.size syscall_entry, . - syscall_entry
