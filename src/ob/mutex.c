#include <ob/mutex.h>
#include <driver/uart.h>

void KeInitializeMutex(PMUTEX Mutex)
{
	KeInitializeSpinLock(&Mutex->Lock);
	Mutex->Signaled = 1;
	Mutex->WaitList.Backward = Mutex->WaitList.Forward = &Mutex->WaitList;
}

void KeiCancelMutexWait(PKPROCESS Process)
{
	LibRemoveListEntry(&Process->WaitList);
	Process->WaitMutex = NULL;
}

void KeiSetMutexSignaled(PMUTEX Mutex)
{
	if (Mutex->WaitList.Backward == &Mutex->WaitList)
	{
		// No process waiting
		Mutex->Signaled = TRUE;
	}
	else
	{
		Mutex->Signaled = FALSE;
		PKPROCESS proc = container_of(Mutex->WaitList.Backward, KPROCESS, WaitList);
		// Cancel the wait (means success)
		KeiCancelMutexWait(proc);
	}
}

void KeSetMutexSignaled(PMUTEX Mutex)
{
	// Although only one process can set the mutex, other process might remove itself from the wait list.
	ObLockObject(Mutex);
	KeiSetMutexSignaled(Mutex);
	ObUnlockObject(Mutex);
}

BOOL KeTestMutexSignaled(PMUTEX Mutex, BOOL Reset)
{
	BOOL ret = FALSE;
	if (Reset)
		ret = Mutex->Signaled;
	else if (Mutex->Signaled)
	{
		ObLockObject(Mutex);
		ret = Mutex->Signaled;
		Mutex->Signaled = FALSE;
		ObUnlockObject(Mutex);
	}
	return ret;
}

APC_ONLY KSTATUS KeWaitForMutexSignaled(PMUTEX Mutex, BOOL Reset) // NOTE: Reset to 1(signaled)
{
	PKPROCESS cur = PsGetCurrentProcess();
	ASSERT(cur->ExecuteLevel == EXECUTE_LEVEL_APC, BUG_BADLEVEL);
	// Disable interrupt to avoid unexpected context switching. (APC level)
	BOOL trapen = arch_disable_trap();
	ObLockObjectFast(Mutex);
	if (!Mutex->Signaled)
	{
		LibInsertListEntry(Mutex->WaitList.Forward, &cur->WaitList);
		cur->WaitMutex = Mutex;
		cur->Status = PROCESS_STATUS_WAIT;
		ObUnlockObjectFast(Mutex);
		KeTaskSwitch();
		ObLockObjectFast(Mutex);
		if (cur->WaitMutex != NULL)
		{
			// Abort
			KeiCancelMutexWait(cur);
			ObUnlockObjectFast(Mutex);
			if (trapen) arch_enable_trap();
			return STATUS_ALERTED;
		}
		ASSERT(Mutex->Signaled == FALSE, BUG_BADLOCK);
	}
	else
	{
		ASSERT(Mutex->WaitList.Backward == &Mutex->WaitList, BUG_BADLOCK);
		Mutex->Signaled = FALSE;
	}
	if (Reset)
		KeiSetMutexSignaled(Mutex);
	ObUnlockObjectFast(Mutex);
	if (trapen) arch_enable_trap();
	return STATUS_SUCCESS;
}
