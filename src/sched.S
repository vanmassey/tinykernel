/* Context switch primitives for the preemptive round-robin scheduler.
 *
 * Every task is entered through an interrupt frame: on boot and on every
 * reschedule the CPU is made to `iretq` into a full frame that the C
 * scheduler has built on that task's stack. That keeps the invariant
 * simple: a task is always interrupted at a frame boundary, and a
 * switch is just "make the current frame the next task's frame".
 */

.section .text
.code64

/* void sched_iret(uint64_t *frame) __attribute__((noreturn))
 *
 * Switch stacks to `frame` and `iretq` into it. Never returns to the
 * caller: the new frame's rip takes over. Used both for the boot
 * handoff into the first task and for the tail end of a reschedule.
 */
.global sched_iret
.type sched_iret, @function
sched_iret:
    movq %rdi, %rsp
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
    addq $16, %rsp      /* vector + error code */
    iretq
.size sched_iret, . - sched_iret

/* void sched_save_fpu(void) - nothing to do until we use the FPU. */
.global sched_save_fpu
.type sched_save_fpu, @function
sched_save_fpu:
    ret
.size sched_save_fpu, . - sched_save_fpu