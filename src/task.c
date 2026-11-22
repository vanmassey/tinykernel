#include "sched.h"

#include "idt.h"
#include "kernel.h"
#include "pmm.h"
#include "pit.h"
#include "string.h"
#include "vmm.h"
#include "vga.h"

/* Enter a prepared interrupt frame on this task's stack; never returns. */
void sched_iret(uint64_t *frame) __attribute__((noreturn));

static struct task tasks[TASK_MAX];
static struct task *current_task;
static struct task *idle_task;
static struct task *ready_head;
static struct task *ready_tail;
static uint64_t context_switches;
static uint32_t next_pid = 1;
static uint64_t irq_stack_top;

static void enqueue_ready(struct task *t)
{
    t->next = 0;
    if (ready_tail) {
        ready_tail->next = t;
    } else {
        ready_head = t;
    }
    ready_tail = t;
}

static struct task *dequeue_ready(void)
{
    struct task *t = ready_head;
    if (!t) {
        return 0;
    }
    ready_head = t->next;
    if (!ready_head) {
        ready_tail = 0;
    }
    t->next = 0;
    return t;
}

/*
 * Build a full interrupt frame on `kstack_top` so that sched_iret()
 * lands in `entry` with `arg` in rdi, as if the task had been
 * preempted exactly there.
 */
static uint64_t *build_frame(uint64_t *kstack_top, void (*entry)(void *), void *arg)
{
    uint64_t *sp = kstack_top;

    *--sp = 0x10;                       /* ss: kernel data */
    *--sp = 0;                          /* rsp: unused for ring-0 entry */
    *--sp = 0x202;                      /* rflags: IF=1, bit1 reserved */
    *--sp = 0x08;                       /* cs: kernel code */
    *--sp = (uint64_t)(uintptr_t)entry; /* rip */
    *--sp = 0;                          /* error code */
    *--sp = 0;                          /* vector */

    *--sp = 0;                          /* rax */
    *--sp = 0;                          /* rbx */
    *--sp = 0;                          /* rcx */
    *--sp = 0;                          /* rdx */
    *--sp = (uint64_t)(uintptr_t)arg;   /* rdi */
    *--sp = 0;                          /* rsi */
    *--sp = 0;                          /* rbp */
    *--sp = 0;                          /* r8 */
    *--sp = 0;                          /* r9 */
    *--sp = 0;                          /* r10 */
    *--sp = 0;                          /* r11 */
    *--sp = 0;                          /* r12 */
    *--sp = 0;                          /* r13 */
    *--sp = 0;                          /* r14 */
    *--sp = 0;                          /* r15 */

    return sp;
}

/* A tiny trampoline stack the boot path switches to before the first
 * task is entered. Each task's own frame lives on its own stack. */

void sched_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    current_task = 0;
    ready_head = ready_tail = 0;
    context_switches = 0;
    next_pid = 1;

    /* Allocate a dedicated IRQ stack so interrupt frames never land on
     * a task stack that is about to be freed or reused. */
    uint64_t irq_stack = (uint64_t)(uintptr_t)pmm_alloc_pages(2);
    irq_stack_top = irq_stack + (2 * PMM_PAGE_SIZE);

    /* Set up per-CPU area for syscall entry (GS base).
     * We use the IRQ stack area: first 8 bytes = user RSP save slot,
     * next 8 bytes = kernel RSP load slot. */
    uint64_t percpu_phys = (uint64_t)(uintptr_t)pmm_alloc_pages(1);
    uint64_t *percpu = (uint64_t *)phys_to_virt(percpu_phys);
    percpu[0] = 0;                    /* user RSP slot */
    percpu[1] = irq_stack_top;        /* kernel RSP slot */
    __asm__ volatile ("wrmsr" : : "c"(0xC0000101), "a"((uint32_t)percpu_phys), "d"(percpu_phys >> 32));
    /* Also set GS base via swapgs in syscall_entry */
}

int sched_spawn(const char *name, void (*entry)(void *), void *arg)
{
    struct task *slot = 0;
    for (unsigned i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state == TASK_FREE) {
            slot = &tasks[i];
            break;
        }
    }
    if (!slot) {
        return -1;
    }

    uint64_t stack_phys = (uint64_t)(uintptr_t)pmm_alloc_pages(KSTACK_SIZE / PMM_PAGE_SIZE);
    if (!stack_phys) {
        return -1;
    }
    uint64_t *kstack = (uint64_t *)phys_to_virt(stack_phys);
    memset(kstack, 0, KSTACK_SIZE);

    slot->pid = next_pid++;
    slot->state = TASK_READY;
    slot->stack_phys = stack_phys;
    slot->kstack_top = kstack + (KSTACK_SIZE / sizeof(uint64_t));
    slot->cr3 = vmm_current_address_space();
    slot->ticks_used = 0;
    slot->sleep_until = 0;
    slot->next = 0;

    if (name) {
        strncpy(slot->name, name, TASK_NAME_MAX - 1);
        slot->name[TASK_NAME_MAX - 1] = '\0';
    }

    slot->rsp = build_frame(slot->kstack_top, entry, arg);
    enqueue_ready(slot);
    return (int)slot->pid;
}

/* Called from the timer IRQ: account time, wake sleepers, preempt. */
void sched_tick(void)
{
    if (!current_task) {
        return;
    }
    current_task->ticks_used++;

    uint64_t now = pit_ticks();
    for (unsigned i = 0; i < TASK_MAX; i++) {
        struct task *t = &tasks[i];
        if (t->state == TASK_SLEEPING && now >= t->sleep_until) {
            t->state = TASK_READY;
            enqueue_ready(t);
        }
    }
}

/*
 * Switch away from the frame that `isr_common` just pushed. That frame
 * is copied onto the task's own stack and becomes its saved context;
 * the stack is then switched to the next task's frame and we iret into
 * it. Called from irq_common with interrupts disabled.
 */
static void save_frame(struct task *t, struct interrupt_frame *frame)
{
    uint64_t *src = (uint64_t *)frame;
    uint64_t *dst = t->kstack_top - (sizeof(struct interrupt_frame) / sizeof(uint64_t));
    for (size_t i = 0; i < sizeof(struct interrupt_frame) / sizeof(uint64_t); i++) {
        dst[i] = src[i];
    }
    t->rsp = dst;
}

void sched_adopt_idle(void)
{
    idle_task = ready_tail;
    if (!idle_task) {
        return;
    }
    if (ready_head == idle_task) {
        ready_head = ready_tail = 0;
    } else {
        struct task *p = ready_head;
        while (p && p->next != idle_task) {
            p = p->next;
        }
        if (p) {
            p->next = 0;
            ready_tail = p;
        }
    }
    idle_task->next = 0;
    idle_task->state = TASK_READY;
}

void sched_schedule(struct interrupt_frame *frame)
{
    struct task *prev = current_task;

    if (!prev) {
        return;
    }
    if (prev == idle_task) {
        if (!ready_head) {
            return;
        }
        save_frame(prev, frame);
        prev->state = TASK_READY;
    } else if (prev->state == TASK_RUNNING && !ready_head) {
        return;
    } else if (prev->state != TASK_DEAD) {
        /* READY means sched_tick already queued a wakeup. Don't queue twice. */
        save_frame(prev, frame);
        if (prev->state == TASK_RUNNING) {
            prev->state = TASK_READY;
            enqueue_ready(prev);
        }
    }

    struct task *next = dequeue_ready();
    if (!next) {
        next = idle_task;
        if (!next || next == prev) {
            return;
        }
    }

    current_task = next;
    next->state = TASK_RUNNING;
    context_switches++;

    if (next->cr3 != vmm_current_address_space()) {
        vmm_set_address_space(next->cr3);
    }

    sched_iret(next->rsp);
}

void sched_yield(void)
{
    /* A voluntary yield from a task body still needs a frame to return
     * to; reuse the task's own saved frame area by forcing a reschedule
     * from the next timer tick instead of doing it inline. */
}

void sched_exit(void) __attribute__((noreturn));
void sched_exit(void)
{
    struct task *t = current_task;
    if (t) {
        t->state = TASK_DEAD;
        if (t->stack_phys) {
            pmm_free_pages((void *)t->stack_phys, KSTACK_SIZE / PMM_PAGE_SIZE);
            t->stack_phys = 0;
        }
    }
    current_task = 0;

    struct task *next = dequeue_ready();
    if (!next) {
        vga_write("\nall tasks finished\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    current_task = next;
    next->state = TASK_RUNNING;
    context_switches++;
    sched_iret(next->rsp);
}

void sched_run(void) __attribute__((noreturn))
{
    struct task *next = dequeue_ready();
    if (!next) {
        vga_write("no tasks to run\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    current_task = next;
    next->state = TASK_RUNNING;
    sched_iret(next->rsp);
}

void sched_sleep(uint64_t ms)
{
    struct task *t = current_task;
    if (!t) {
        return;
    }
    uint64_t ticks = (ms * pit_hz()) / 1000u;
    if (ticks == 0) {
        ticks = 1;
    }
    t->sleep_until = pit_ticks() + ticks;
    t->state = TASK_SLEEPING;

    /* Reschedule on the next tick: a sleeping task has already saved a
     * frame on its own stack, so the timer will not touch it again. */
    (void)t;
}

struct task *sched_current(void)
{
    return current_task;
}

uint64_t sched_context_switches(void)
{
    return context_switches;
}

unsigned sched_ready_count(void)
{
    unsigned n = 0;
    for (struct task *t = ready_head; t; t = t->next) {
        n++;
    }
    return n;
}

uint64_t sched_irq_stack_top(void)
{
    return irq_stack_top;
}

void sched_dump(void)
{
    vga_printf("  pid  state     ticks  name\n");
    for (unsigned i = 0; i < TASK_MAX; i++) {
        struct task *t = &tasks[i];
        if (t->state == TASK_FREE) {
            continue;
        }
        const char *state = "?";
        switch (t->state) {
        case TASK_READY:    state = "ready";    break;
        case TASK_RUNNING:  state = "running";  break;
        case TASK_SLEEPING: state = "sleeping"; break;
        case TASK_DEAD:     state = "dead";     break;
        default: break;
        }
        vga_printf("  %-4lu %-9s %6lu  %s\n",
                   (unsigned long)t->pid, state,
                   (unsigned long)t->ticks_used, t->name);
    }
}

int task_spawn_user(const char *name, uint64_t entry, uint64_t ustack_top)
{
    struct task *slot = 0;
    for (unsigned i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state == TASK_FREE) {
            slot = &tasks[i];
            break;
        }
    }
    if (!slot) {
        return -1;
    }

    uint64_t stack_phys = (uint64_t)(uintptr_t)pmm_alloc_pages(KSTACK_SIZE / PMM_PAGE_SIZE);
    if (!stack_phys) {
        return -1;
    }
    uint64_t *kstack = (uint64_t *)phys_to_virt(stack_phys);
    memset(kstack, 0, KSTACK_SIZE);

    slot->pid = next_pid++;
    slot->state = TASK_READY;
    slot->stack_phys = stack_phys;
    slot->kstack_top = kstack + (KSTACK_SIZE / sizeof(uint64_t));
    slot->cr3 = vmm_current_address_space();
    slot->ticks_used = 0;
    slot->sleep_until = 0;
    slot->next = 0;

    if (name) {
        strncpy(slot->name, name, TASK_NAME_MAX - 1);
        slot->name[TASK_NAME_MAX - 1] = '\0';
    }

    /* Build a sysret frame on the kernel stack for first entry. */
    uint64_t *sp = slot->kstack_top;
    *--sp = 0x23;                  /* ss: user data */
    *--sp = ustack_top;            /* rsp: user stack top */
    *--sp = 0x202;                 /* rflags: IF=1 */
    *--sp = 0x1B;                  /* cs: user code (0x18|3) */
    *--sp = entry;                 /* rip: user entry point */
    slot->rsp = sp;

    enqueue_ready(slot);
    return (int)slot->pid;
}