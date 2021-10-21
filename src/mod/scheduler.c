#include <mod/scheduler.h>
#include <driver/uart.h>

extern PKPROCESS KernelProcess;
// Although one core has only one active list, locks are no longer needed.
// But RT level is still needed to avoid being interrupted.
// static SPINLOCK ActiveListLock;
// static SPINLOCK InactiveListLock;
static LIST_ENTRY InactiveList;
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
	InactiveList.Forward = InactiveList.Backward = &InactiveList;
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
	Process->Status = PROCESS_STATUS_RUNABLE;
	// KeReleaseSpinLockFast(&ActiveListLock);
}

void PsiExitProcess()
{
	arch_disable_trap();
	PsGetCurrentProcess()->Status = PROCESS_STATUS_ZOMBIE;
	KeTaskSwitch();
}

void PsiProcessEntry()
{
	// Interrupt disabled. Keep disabled when return.
	// Do DPC & APCs.
	EXECUTE_LEVEL el = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	KeLowerExecuteLevel(el);
	return;
}

void KiClockTrapEntry()
{
	reset_clock(TIME_SLICE_MS);
	//uart_put_char('t');
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->ExecuteLevel >= EXECUTE_LEVEL_RT)
		return;
	KeTaskSwitch();
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

UNSAFE RT_ONLY void KeClearDpcList()
{
	// Batch processing
	KeAcquireSpinLockFast(&DpcListLock);
	PDPC_ENTRY dpc = DpcList;
	DpcList = NULL;
	KeReleaseSpinLockFast(&DpcListLock);
	if (dpc == NULL)
		return;
	arch_enable_trap();
	for (PDPC_ENTRY p = dpc; p != NULL; p = p->NextEntry)
	{
		p->DpcRoutine(p->DpcArgument);
	}
	arch_disable_trap();
	for (PDPC_ENTRY p = dpc; p != NULL;)
	{
		PDPC_ENTRY np = p->NextEntry;
		MmFreeObject(&DpcObjectPool, (PVOID)p);
		p = np;
	}
}

UNSAFE APC_ONLY void KeClearApcList() // WARNING: MUST be called in APC level
{
	PKPROCESS cur = PsGetCurrentProcess();
	// Batch processing
	ObLockObjectFast(cur);
	PAPC_ENTRY apc = cur->ApcList;
	cur->ApcList = NULL;
	ObUnlockObjectFast(cur);
	if (apc == NULL)
		return;
	arch_enable_trap();
	for (PAPC_ENTRY p = apc; p != NULL; p = p->NextEntry)
	{
		p->ApcRoutine(p->ApcArgument);
	}
	arch_disable_trap();
	for (PAPC_ENTRY p = apc; p != NULL;)
	{
		PAPC_ENTRY np = p->NextEntry;
		MmFreeObject(&ApcObjectPool, (PVOID)p);
		p = np;
	}
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
		if (DpcList != NULL)
		{
			proc->ExecuteLevel = EXECUTE_LEVEL_RT;
			BOOL trapen = arch_disable_trap();
			KeClearDpcList();
			if (trapen) arch_enable_trap();
		}
	}
	if (proc->ExecuteLevel >= EXECUTE_LEVEL_APC && OriginalExecuteLevel < EXECUTE_LEVEL_APC)
	{
		if (proc->ApcList != NULL)
		{
			proc->ExecuteLevel = EXECUTE_LEVEL_APC;
			BOOL trapen = arch_disable_trap();
			KeClearApcList();
			if (trapen) arch_enable_trap();
		}
	}
	proc->ExecuteLevel = OriginalExecuteLevel;
	// ObUnlockObject(proc);
}

UNSAFE void KeTaskSwitch()
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
	switch (nxt->Status)
	{
		case PROCESS_STATUS_RUNABLE:
			nxt->Status = PROCESS_STATUS_RUNNING;
			break;
		default:
			// TODO: PANIC
			break;
	}
	switch (cur->Status)
	{
		case PROCESS_STATUS_RUNNING:
			cur->Status = PROCESS_STATUS_RUNABLE;
			break;
		case PROCESS_STATUS_ZOMBIE:
			LibRemoveListEntry(&cur->SchedulerList);
			break;
		case PROCESS_STATUS_WAIT:
			LibRemoveListEntry(&cur->SchedulerList);
			LibInsertListEntry(&InactiveList, &cur->SchedulerList);
			break;
		default:
			// TODO: PANIC
			break;
	}
	// KeReleaseSpinLockFast(&ActiveListLock);
	cur->Context.UserStack = (PVOID)arch_get_usp();
	arch_set_usp((ULONG64)nxt->Context.UserStack);
	MmSwitchMemorySpaceEx(cur->MemorySpace, nxt->MemorySpace);
	arch_set_tid((ULONG64)nxt);
	swtch(nxt->Context.KernelStack.p, &cur->Context.KernelStack.p);
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	KeLowerExecuteLevel(oldel);
	uart_put_char('/');
	uart_put_char('0' + cur->ProcessId);
}

RT_ONLY void PsiCheckInactiveList()
{
	PLIST_ENTRY p = InactiveList.Backward;
	for (;;)
	{
		PLIST_ENTRY np = p->Backward;
		if (p == &InactiveList)
			break;
		PKPROCESS proc = container_of(p, KPROCESS, SchedulerList);
		if (proc->ApcList != NULL ||    // Alerted
			proc->WaitMutex == NULL)    // Signaled
		{
			proc->Status = PROCESS_STATUS_RUNABLE;
			LibRemoveListEntry(&proc->SchedulerList);
			LibInsertListEntry(&KernelProcess->SchedulerList, &proc->SchedulerList);
			uart_put_char('+');
			uart_put_char('0' + proc->ProcessId);
		}
		p = np;
	}
}
