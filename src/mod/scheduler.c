#include <mod/scheduler.h>
#include <driver/uart.h>

extern PKPROCESS KernelProcess;
// Although one core has only one active list, locks are no longer needed.
// But RT level is still needed to avoid being interrupted.
// static SPINLOCK ActiveListLock;
static SPINLOCK DpcListLock;
static PDPC_ENTRY DpcList;
static OBJECT_POOL ApcObjectPool, DpcObjectPool;

void swtch (PVOID kstack, PVOID* oldkstack);
void KiClockTrapEntry();

void ObInitializeScheduler()
{
	// KeInitializeSpinLock(&ActiveListLock);
	KeInitializeSpinLock(&DpcListLock);
	MmInitializeObjectPool(&ApcObjectPool, sizeof(APC_ENTRY));
	MmInitializeObjectPool(&DpcObjectPool, sizeof(DPC_ENTRY));
	DpcList = NULL;
	arch_set_tid((ULONG64)KernelProcess);
	KernelProcess->Status = PROCESS_STATUS_RUNNING;
	init_clock();
	set_clock_handler(KiClockTrapEntry);
	init_trap();
}

RT_ONLY void PsiStartNewProcess(PKPROCESS Process)
{
	// KeAcquireSpinLockFast(&ActiveListLock);
	LibInsertListEntry(&PsGetCurrentProcess()->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_WAITING;
	// After that, the process is visible in the process list, and MIGHT BE REFERENCED BY OTHERS.
	// KeReleaseSpinLockFast(&ActiveListLock);
}

RT_ONLY void PsiExitProcess()
{
	PKPROCESS cur = PsGetCurrentProcess();
	// KeAcquireSpinLockFast(&ActiveListLock);
	LibRemoveListEntry(cur->SchedulerList);
	// KeReleaseSpinLockFast(&ActiveListLock);
	cur->Status = PROCESS_STATUS_ZOMBIE;
	KeTaskSwitch();
}

void KiClockTrapEntry()
{
	reset_clock(TIME_SLICE_MS);
	//uart_put_char('t');
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->ExecuteLevel >= EXECUTE_LEVEL_RT)
		return;
	// Raise EL to RT level and enable interrupt
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	arch_enable_trap();
	KeClearDpcList();
	KeTaskSwitch();
	if (oldel < EXECUTE_LEVEL_APC)
	{
		cur->ExecuteLevel = EXECUTE_LEVEL_APC;
		KeClearApcList();
	}
	arch_disable_trap();
	cur->ExecuteLevel = oldel;
	return;
}

PDPC_ENTRY KeCreateDpc(PDPC_ROUTINE Routine, ULONG64 Argument)
{
	BOOL trapen = arch_disable_trap();
	PDPC_ENTRY p = (PDPC_ENTRY)MmAllocateObject(&DpcObjectPool);
	if (p != NULL)
	{
		p->DpcRoutine = Routine;
		p->DpcArgument = Argument;
		KeAcquireSpinLockFast(&DpcListLock);
		p->NextEntry = DpcList;
		DpcList = p;
		KeReleaseSpinLockFast(&DpcListLock);
	}
	if (trapen)
		arch_enable_trap();
	return p;
}

PAPC_ENTRY KeCreateApcEx(PKPROCESS Process, PAPC_ROUTINE Routine, ULONG64 Argument)
{
	BOOL trapen = arch_disable_trap();
	PAPC_ENTRY p = (PAPC_ENTRY)MmAllocateObject(&ApcObjectPool);
	if (p != NULL)
	{
		p->ApcRoutine = Routine;
		p->ApcArgument = Argument;
		ObLockObjectFast(Process);
		p->NextEntry = Process->ApcList;
		Process->ApcList = p;
		ObUnlockObjectFast(Process);
	}
	if (trapen)
		arch_enable_trap();
	return p;
}

RT_ONLY void KeClearDpcList()
{
	if (DpcList == NULL)
		return;
	// Batch processing
	KeAcquireSpinLock(&DpcListLock);
	PDPC_ENTRY dpc = DpcList;
	DpcList = NULL;
	KeReleaseSpinLock(&DpcListLock);
	if (dpc == NULL)
		return;
	for (PDPC_ENTRY p = dpc; p != NULL; p = p->NextEntry)
	{
		p->DpcRoutine(p->DpcArgument);
	}
	BOOL trapen = arch_disable_trap();
	for (PDPC_ENTRY p = dpc; p != NULL;)
	{
		PDPC_ENTRY np = p->NextEntry;
		MmFreeObject(&DpcObjectPool, (PVOID)p);
		p = np;
	}
	if (trapen)
		arch_enable_trap();
}

APC_ONLY void KeClearApcList() // WARNING: MUST be called in APC level
{
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->ApcList == NULL)
		return;
	// Batch processing
	ObLockObject(cur);
	PAPC_ENTRY apc = cur->ApcList;
	cur->ApcList = NULL;
	ObUnlockObject(cur);
	if (apc == NULL)
		return;
	for (PAPC_ENTRY p = apc; p != NULL; p = p->NextEntry)
	{
		p->ApcRoutine(p->ApcArgument);
	}
	BOOL trapen = arch_disable_trap();
	for (PAPC_ENTRY p = apc; p != NULL;)
	{
		PAPC_ENTRY np = p->NextEntry;
		MmFreeObject(&ApcObjectPool, (PVOID)p);
		p = np;
	}
	if (trapen)
		arch_enable_trap();
}

// No need to lock the process, since only the process itself can change its realtime-state.
EXECUTE_LEVEL KeRaiseExecuteLevel(EXECUTE_LEVEL TargetExecuteLevel)
{
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	EXECUTE_LEVEL old = proc->ExecuteLevel;
	proc->ExecuteLevel = TargetExecuteLevel;
	// ObUnlockObject(proc);
	return old;
}

// The OldState must be the one returned by raise()!
// Otherwise, the kstack and trapframe would be corruptted! 
void KeLowerExecuteLevel(EXECUTE_LEVEL OriginalExecuteLevel)
{
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	if (proc->ExecuteLevel >= EXECUTE_LEVEL_RT && OriginalExecuteLevel < EXECUTE_LEVEL_RT)
	{
		proc->ExecuteLevel = EXECUTE_LEVEL_RT;
		KeClearDpcList();
	}
	if (proc->ExecuteLevel >= EXECUTE_LEVEL_APC && OriginalExecuteLevel < EXECUTE_LEVEL_APC)
	{
		proc->ExecuteLevel = EXECUTE_LEVEL_APC;
		KeClearApcList();
	}
	proc->ExecuteLevel = OriginalExecuteLevel;
	// ObUnlockObject(proc);
}

RT_ONLY void KeTaskSwitch() // MUST be called in RT level
{
	// Find the next
	// No need to lock the process, since only the scheduler can change its status.
	PKPROCESS cur = PsGetCurrentProcess();
	// KeAcquireSpinLockFast(&ActiveListLock);
	PKPROCESS nxt = container_of(cur->SchedulerList.Backward, KPROCESS, SchedulerList);
	if (nxt == cur) // No more process
	{
		// KeReleaseSpinLockFast(&ActiveListLock);
		return;
	}
	// Do switch
	nxt->Status = PROCESS_STATUS_RUNNING;
	if (cur->Status == PROCESS_STATUS_RUNNING)
		cur->Status = PROCESS_STATUS_WAITING;
	else // For zombie, blocked and other inactive processes
		LibRemoveListEntry(&cur->SchedulerList);
	// KeReleaseSpinLockFast(&ActiveListLock);
	BOOL trapen = arch_disable_trap();
	cur->Context.UserStack = (PVOID)arch_get_usp();
	arch_set_usp((ULONG64)nxt->Context.UserStack);
	MmSwitchMemorySpaceEx(cur->MemorySpace, nxt->MemorySpace);
	arch_set_tid((ULONG64)nxt);
	swtch(nxt->Context.KernelStack.p, &cur->Context.KernelStack.p);
	if (trapen)
		arch_enable_trap();
}

