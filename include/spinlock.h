#pragma once

/*
 * A test-and-set spinlock. Locking is only ever required once the
 * scheduler can preempt a task while it holds kernel data, so callers
 * must take the lock with interrupts disabled on the fast path.
 *
 * `locked` is 0 (unlocked) or 1 (held).
 */
struct spinlock {
    volatile int locked;
};

#define SPINLOCK_INIT { 0 }

#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x)   __builtin_expect(!!(x), 1)
#else
#define unlikely(x) (x)
#define likely(x)   (x)
#endif

void spin_lock(struct spinlock *lock);
void spin_unlock(struct spinlock *lock);