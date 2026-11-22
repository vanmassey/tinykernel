/* ISR / IRQ stubs + syscall entry for x86_64 */

.section .text
.code64

.macro ISR_NOERR vec
.global isr\vec
isr\vec:
    pushq $0
    pushq $\vec
    jmp isr_common
.endm

.macro ISR_ERR vec
.global isr\vec
isr\vec:
    pushq $\vec
    jmp isr_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

.global isr_common
isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    movq %rsp, %r12
    andq $-16, %rsp
    cld                         /* C code expects the direction flag to be clear. */
    call exception_handler
    movq %r12, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp
    iretq

.macro IRQ n
.global irq\n
irq\n:
    pushq $0
    pushq $(\n + 32)
    jmp irq_common
.endm

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

.global irq_common
irq_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call irq_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp
    iretq

/* Per-CPU area for syscall entry (GS base points here) */
.section .bss
.percpu:
.global percpu_syscall_rsp
percpu_syscall_rsp:
    .skip 16
.previous

/* Syscall entry via SYSCALL instruction.
 * On entry: RCX = user RIP, R11 = user RFLAGS, RSP = user RSP
 * Registers per System V AMD64 ABI: RDI, RSI, RDX, R10, R8, R9
 * Syscall number in RAX.
 * We save user state, switch to kernel stack, call C handler,
 * then restore and SYSRET back. */
.section .text
.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    /* Swap to kernel stack using per-CPU area (GS base) */
    swapgs
    movq %rsp, %gs:percpu_syscall_rsp    /* save user RSP */
    movq %gs:percpu_syscall_rsp+8, %rsp  /* load kernel RSP */

    /* Build frame matching interrupt_frame layout (for consistency) */
    pushq $0x23              /* ss (user data selector) */
    pushq percpu_syscall_rsp(%gs)  /* rsp (user) */
    pushq %r11               /* rflags (user) */
    pushq $0x1B              /* cs (user code selector | 3) */
    pushq %rcx               /* rip (user) */

    /* Save syscall arguments before pushing more registers */
    pushq %r10               /* arg5 / a3 */
    pushq %rdx               /* arg3 / a2 */
    pushq %rsi               /* arg2 / a1 */
    pushq %rdi               /* arg1 / a0 */
    pushq %rax               /* syscall number */

    /* Save remaining callee-saved registers */
    pushq %rbx
    pushq %rcx
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Stack layout (RSP points to r15):
     * 0:   r15
     * 8:   r14
     * 16:  r13
     * 24:  r12
     * 32:  r11
     * 40:  r9
     * 48:  r8
     * 56:  rbp
     * 64:  rcx
     * 72:  rbx
     * 80:  num (rax)
     * 88:  a0 (rdi)
     * 96:  a1 (rsi)
     * 104: a2 (rdx)
     * 112: a3 (r10)
     * 120: rip (rcx)
     * 128: cs
     * 136: rflags (r11)
     * 144: rsp (user)
     * 152: ss */

    movq 80(%rsp), %rdi      /* num */
    movq 88(%rsp), %rsi      /* a0 */
    movq 96(%rsp), %rdx      /* a1 */
    movq 104(%rsp), %rcx     /* a2 */
    movq 112(%rsp), %r8      /* a3 */

    call syscall_handler

    /* Return value in RAX */

    /* Restore callee-saved registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r9
    popq %r8
    popq %rbp
    popq %rcx
    popq %rbx

    /* Discard saved args */
    addq $40, %rsp

    /* Restore user RIP, RFLAGS, RSP via SYSRET */
    /* Stack now has: rip, cs, rflags, rsp, ss */
    swapgs
    sysretq
.size syscall_entry, . - syscall_entry