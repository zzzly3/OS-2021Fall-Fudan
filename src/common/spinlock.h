#pragma once

#ifndef _COMMON_SPINLOCK_H_
#define _COMMON_SPINLOCK_H_

#include <common/defines.h>
#include <aarch64/intrinsic.h>

typedef struct {
    volatile bool locked;
    bool enable_trap;
} SpinLock;

void init_spinlock(SpinLock *lock);
bool try_acquire_spinlock(SpinLock *lock);
void acquire_spinlock(SpinLock *lock);
void release_spinlock(SpinLock *lock);
void wait_spinlock(SpinLock *lock);
bool holding_spinlock(SpinLock *lock);

typedef SpinLock SPINLOCK, *PSPINLOCK;
#define KeInitializeSpinLock init_spinlock
#define KeTryToAcquireSpinLockFast try_acquire_spinlock
#define KeAcquireSpinLockFast acquire_spinlock
#define KeReleaseSpinLockFast release_spinlock
#define KeWaitForSpinLockFast wait_spinlock
#define KeTestSpinLock(lock) (lock->locked)
#define KeAcquireSpinLock(lock) ({ \
    bool __t = arch_disable_trap(); \
    acquire_spinlock(lock); \
    (lock)->enable_trap = __t;})
#define KeReleaseSpinLock(lock) ({ \
    bool __t = (lock)->enable_trap; \
    release_spinlock(lock); \
    if (__t) arch_enable_trap();})

#endif
