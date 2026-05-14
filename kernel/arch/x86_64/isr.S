/*
 * Oxygen kernel - x86_64 interrupt service routine stubs.
 *
 * Per-vector entry points (`isr0`..`isr31`) build a uniform stack frame
 * (struct interrupt_frame) and tail-call into `isr_common`, which saves
 * GPRs and calls the C-level handler.
 */

.section .text

/* ----------------------------------------------------------------------
 * Per-vector stub generator.
 *   err = 1 when the CPU pushes a hardware error code for this vector,
 *         0 otherwise (we push a fake 0 so isr_common sees a uniform frame).
 * ---------------------------------------------------------------------- */
.macro ISR vec, err
.global isr\vec
.type   isr\vec, @function
isr\vec:
    .if \err == 0
    pushq $0
    .endif
    pushq $\vec
    jmp   isr_common
.size   isr\vec, . - isr\vec
.endm

/* Vectors that hardware pushes an error code for: 8, 10-14, 17, 21. */
ISR 0,  0
ISR 1,  0
ISR 2,  0
ISR 3,  0
ISR 4,  0
ISR 5,  0
ISR 6,  0
ISR 7,  0
ISR 8,  1
ISR 9,  0
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 15, 0
ISR 16, 0
ISR 17, 1
ISR 18, 0
ISR 19, 0
ISR 20, 0
ISR 21, 1
ISR 22, 0
ISR 23, 0
ISR 24, 0
ISR 25, 0
ISR 26, 0
ISR 27, 0
ISR 28, 0
ISR 29, 0
ISR 30, 0
ISR 31, 0

/* IRQ vectors after PIC remap to 0x20..0x2F.
 * IRQ0 = timer (PIT), IRQ1 = keyboard, IRQ2 = cascade, etc. */
ISR 32, 0
ISR 33, 0
ISR 34, 0
ISR 35, 0
ISR 36, 0
ISR 37, 0
ISR 38, 0
ISR 39, 0
ISR 40, 0
ISR 41, 0
ISR 42, 0
ISR 43, 0
ISR 44, 0
ISR 45, 0
ISR 46, 0
ISR 47, 0


/* ----------------------------------------------------------------------
 * Common path: save GPRs, hand off to the C dispatcher with a pointer
 * to the interrupt frame, restore on return. Dispatcher fans out to
 * either the exception path or the IRQ path based on vector.
 * ---------------------------------------------------------------------- */
.type isr_common, @function
isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    cld                         /* SysV ABI: DF clear on entry to C code */
    mov   %rsp, %rdi            /* arg 0: &interrupt_frame */
    call  interrupt_dispatch

    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %r11
    popq  %r10
    popq  %r9
    popq  %r8
    popq  %rdi
    popq  %rsi
    popq  %rbp
    popq  %rdx
    popq  %rcx
    popq  %rbx
    popq  %rax

    addq  $16, %rsp             /* discard vector + error code */
    iretq
.size isr_common, . - isr_common


/* ----------------------------------------------------------------------
 * Table of stub addresses, indexed by vector. Used by idt_init().
 * ---------------------------------------------------------------------- */
.section .rodata
.align 8
.global isr_stubs
isr_stubs:
.irp v, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    .quad isr\v
.endr
