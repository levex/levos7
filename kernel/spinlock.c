#include <levos/spinlock.h>
#include <levos/arch.h>
#include <levos/task.h>

void
spin_lock_init(spinlock_t *l)
{
    l->value = 0;
    l->holder = NULL;
}

void
spin_lock(spinlock_t *l)
{
    arch_spin_lock(&l->value);
    if (current_task)
        l->holder = current_task;
}

void
spin_unlock(spinlock_t *l)
{
    l->holder = NULL;
    arch_spin_unlock(&l->value);
}

int
spin_lock_would_deadlock(spinlock_t *lock)
{
    if (lock->holder && lock->holder == current_task)
        return 1;

    return 0;
}
