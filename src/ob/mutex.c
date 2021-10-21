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
		uart_put_char('*');
		uart_put_char('0' + proc->ProcessId);
	}
}

void KeSetMutexSignaled(PMUTEX Mutex)
{
	// Although only one process can set the mutex, other process might remove itself from the wait list.
	ObLockObject(Mutex);
	KeiSetMutexSignaled(Mutex);
	ObUnlockObject(Mutex);
}

APC_ONLY KSTATUS KeWaitForMutexSignaled(PMUTEX Mutex, BOOL Reset) // NOTE: Reset to 1(signaled)
{
	PKPROCESS cur = PsGetCurrentProcess();
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
		uart_put_char('$');
		uart_put_char('0' + cur->ProcessId);
		ObLockObjectFast(Mutex);
		if (cur->WaitMutex != NULL)
		{
			// Abort
			KeiCancelMutexWait(cur);
			ObUnlockObjectFast(Mutex);
			if (trapen) arch_enable_trap();
			return STATUS_ALERTED;
		}
	}
	if (Reset)
		KeiSetMutexSignaled(Mutex);
	ObUnlockObjectFast(Mutex);
	if (trapen) arch_enable_trap();
	return STATUS_SUCCESS;
}
