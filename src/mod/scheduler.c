#include <mod/scheduler.h>

extern PKPROCESS KernelProcess;
// Although one core has only one active list, locks are needed to apply the cross-core stealing policy (TODO).
static SPINLOCK ActiveListLock;

void swtch (PVOID kstack, PVOID* oldkstack);

void ObInitializeScheduler()
{
	KeInitializeSpinLock(ActiveListLock);
	arch_set_tid((ULONG64)KernelProcess);
	KernelProcess->Status = PROCESS_STATUS_RUNNING;
}

void PsiStartNewProcess(PKPROCESS Process)
{
	KeAcquireSpinLock(&ActiveListLock);
	LibInsertListEntry(&PsGetCurrentProcess()->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_WAITING;
	// After that, the process is visible in the process list, and MIGHT BE REFERENCED BY OTHERS.
	KeReleaseSpinLock(&ActiveListLock);
}

// No need to lock the process, since only the process itself can change its realtime-state.
void KeRaiseRealTimeState(BOOL* OldState)
{
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	*OldState = proc->RealTimeState;
	proc->RealTimeState = TRUE;
	// ObUnlockObject(proc);
}

// The OldState must be the one returned by raise()!
// Otherwise, the kstack and trapframe would be corruptted! 
void KeLowerRealTimeState(BOOL OldState)
{
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	proc->RealTimeState = OldState;
	// ObUnlockObject(proc);
}

void KeTaskSwitch()
{
	// Find the next
	// No need to lock the process, since only the scheduler can change its status.
	PKPROCESS cur = PsGetCurrentProcess();
	// TODO: if cur is realtime, core fault!
	KeAcquireSpinLock(&ActiveListLock);
	PKPROCESS nxt = container_of(cur->SchedulerList.Backward, KPROCESS, SchedulerList);
	if (nxt == cur) // No more process
	{
		KeReleaseSpinLock(&ActiveListLock);
		return;
	}
	// Do switch
	arch_disable_trap();
	nxt->Status = PROCESS_STATUS_RUNNING;
	KeReleaseSpinLock(&ActiveListLock);
	cur->Context.UserStack = (PVOID)arch_get_usp();
	arch_set_usp((ULONG64)nxt->Context.UserStack);
	MmSwitchMemorySpaceEx(cur->MemorySpace, nxt->MemorySpace);
	arch_set_tid((ULONG64)nxt);
	cur->Status = PROCESS_STATUS_WAITING;
	arch_enable_trap();
	swtch(nxt->Context.KernelStack.p, &cur->Context.KernelStack.p);
}