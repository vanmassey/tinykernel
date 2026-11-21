#pragma once

#include <stddef.h>
#include <stdint.h>

#include "idt.h"

#define SCHED_HZ        1000u
#define KSTACK_SIZE     (16 * 1024)
#define TASK_NAME_MAX   16
#define TASK_MAX        32

enum task_state {
    TASK_FREE = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_DEAD,
};

struct task {
    uint32_t pid;
    enum task_state state;
    char name[TASK_NAME_MAX];
    uint64_t *rsp;          /* saved stack pointer, or the initial frame */
    uint64_t cr3;           /* this task's PML4 */
    uint64_t stack_phys;    /* backing store for the kernel stack */
    uint64_t ticks_used;    /* how many timer ticks this task has run */
    uint64_t sleep_until;   /* wake-up tick for TASK_SLEEPING */
    uint64_t *kstack_top;   /* one-past-the-end of the kernel stack */
    struct task *next;
};

void sched_init(void);
int sched_spawn(const char *name, void (*entry)(void *), void *arg);
/* Last spawned task becomes the idle thread and leaves the ready queue. */
void sched_adopt_idle(void);
void sched_tick(void);
void sched_yield(void);
void sched_exit(void) __attribute__((noreturn));
void sched_run(void) __attribute__((noreturn));
void sched_sleep(uint64_t ms);

struct task *sched_current(void);
uint64_t sched_context_switches(void);
unsigned sched_ready_count(void);
void sched_dump(void);
uint64_t sched_irq_stack_top(void);

/* User-mode task support */
int task_spawn_user(const char *name, uint64_t entry, uint64_t ustack_top);

/* Used by the switch to pick the next task; implemented in sched.c. */
void sched_schedule(struct interrupt_frame *frame);