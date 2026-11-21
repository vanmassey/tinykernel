#include "spinlock.h"

void spin_lock(struct spinlock *lock)
{
    if (unlikely(!lock)) {
        return;
    }
    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
            __asm__ volatile ("pause");
        }
    }
}

void spin_unlock(struct spinlock *lock)
{
    if (unlikely(!lock)) {
        return;
    }
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
}