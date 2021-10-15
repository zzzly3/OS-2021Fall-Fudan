#pragma once

#ifndef _COMMON_SPINLOCK_H_
#define _COMMON_SPINLOCK_H_

#include <common/defines.h>

typedef struct {
    volatile bool locked;
} SpinLock;

void init_spinlock(SpinLock *lock);
bool try_acquire_spinlock(SpinLock *lock);
void acquire_spinlock(SpinLock *lock);
void release_spinlock(SpinLock *lock);
void wait_spinlock(SpinLock *lock);

typedef SpinLock SPINLOCK, *PSPINLOCK;
#define KeInitializeSpinLock init_spinlock
#define KeTryToAcquireSpinLock try_acquire_spinlock
#define KeAcquireSpinLock acquire_spinlock
#define KeReleaseSpinLock release_spinlock
#define KeWaitForSpinLock wait_spinlock
#define KeTestSpinLock(lock) (lock->locked)

// TODO: scheduler-related lock

#endif
