#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <common/lutil.h>
#include <common/spinlock.h>
#include <ob/proc.h>

typedef struct _MUTEX
{
	BOOL Signaled;
	SPINLOCK Lock;
	LIST_ENTRY WaitList;
} MUTEX, *PMUTEX;

void KeSetMutexSignaled(PMUTEX);
APC_ONLY KSTATUS KeWaitForMutexSignaled(PMUTEX, BOOL);
void KeInitializeMutex(PMUTEX);

#endif