#include <mod/scheduler.h>

extern BOOL KeBugFaultFlag;
extern PKPROCESS KernelProcess[CPU_NUM];
// Although one core has only one active list, locks are no longer needed.
// But RT level is still needed to avoid being interrupted.
// static SPINLOCK ActiveListLock;
// static SPINLOCK InactiveListLock;
static LIST_ENTRY InactiveList[CPU_NUM];
int DpcWatchTimer[CPU_NUM];
PDPC_ENTRY DpcList;
static SPINLOCK DpcListLock;
static OBJECT_POOL ApcObjectPool, DpcObjectPool;
int ActiveProcessCount[CPU_NUM];
PKPROCESS TransferProcess;
int WorkerSwitchTimer[CPU_NUM];
static SPINLOCK TransferListLock;

void swtch (PVOID kstack, PVOID* oldkstack);
void KiClockTrapEntry();

void ObInitializeScheduler()
{
	int cid = cpuid();
	// KeInitializeSpinLock(&ActiveListLock);
	if (cid == START_CPU)
	{
		KeInitializeSpinLock(&DpcListLock);
		KeInitializeSpinLock(&TransferListLock);
		MmInitializeObjectPool(&ApcObjectPool, sizeof(APC_ENTRY));
		MmInitializeObjectPool(&DpcObjectPool, sizeof(DPC_ENTRY));
		DpcList = NULL;
		TransferProcess = NULL;
	}
	WorkerSwitchTimer[cid] = -1;
	ActiveProcessCount[cid] = 1;
	DpcWatchTimer[cid] = -1;
	InactiveList[cid].Forward = InactiveList[cid].Backward = &InactiveList[cid];
	arch_set_tid((ULONG64)KernelProcess[cid]);
	KernelProcess[cid]->Status = PROCESS_STATUS_RUNNING;
	init_clock();
	set_clock_handler(KiClockTrapEntry);
	init_trap();
}


RT_ONLY void PsiStartNewProcess(PKPROCESS Process)
{
	// KeAcquireSpinLockFast(&ActiveListLock);
	LibInsertListEntry(&PsGetCurrentProcess()->SchedulerList, &Process->SchedulerList);
	ActiveProcessCount[cpuid()]++;
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

RT_ONLY BOOL PsiTransferProcess()
{
	// No need for precision
	int maxn = 0, minn = ~(1 << 31), cid = cpuid(), mn = ActiveProcessCount[cid];
	for (int i = 0; i < CPU_NUM; i++)
		maxn = max(maxn, ActiveProcessCount[i]), minn = min(minn, ActiveProcessCount[i]);
	if (maxn > minn)
	{
		if (mn == maxn && mn > 2)
		{
			// Push out
			KeAcquireSpinLockFast(&TransferListLock);
			if (TransferProcess == NULL)
			{
				PKPROCESS p = container_of(KernelProcess[cid]->SchedulerList.Forward, KPROCESS, SchedulerList);
				LibRemoveListEntry(&p->SchedulerList);
				ActiveProcessCount[cid]--;
				TransferProcess = p;
			}
			KeReleaseSpinLockFast(&TransferListLock);
		}
		else if (mn == minn)
		{
			// Accept
			KeAcquireSpinLockFast(&TransferListLock);
			if (TransferProcess != NULL)
			{
				PKPROCESS p = TransferProcess;
				TransferProcess = NULL;
				LibInsertListEntry(&KernelProcess[cid]->SchedulerList, &p->SchedulerList);
				ActiveProcessCount[cid]++;
			}
			KeReleaseSpinLockFast(&TransferListLock);
		}
	}
}

void KiClockTrapEntry()
{
	if (KeBugFaultFlag) for(arch_disable_trap();;);
	reset_clock(TIME_SLICE_MS);
	//uart_put_char('t');
	PKPROCESS cur = PsGetCurrentProcess();
	int cid = cpuid();
	if (cur->ExecuteLevel >= EXECUTE_LEVEL_RT)
	{
		//printf("cpu %d watch %d\n", cid, DpcWatchTimer[cid]);
		ASSERT(DpcWatchTimer[cid] != 0, BUG_BADDPC);
		if (DpcWatchTimer[cid] != -1)
			DpcWatchTimer[cid]--;
		return;
	}
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
	int cid = cpuid();
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
		DpcWatchTimer[cid] = 2;
		p->DpcRoutine(p->DpcArgument);
	}
	arch_disable_trap();
	DpcWatchTimer[cid] = -1;
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
	// If trap enabled, should use atomic bit-set operation instead.
	cur->Flags |= PROCESS_FLAG_APCSTATE;
	arch_enable_trap();
	for (PAPC_ENTRY p = apc; p != NULL; p = p->NextEntry)
	{
		p->ApcRoutine(p->ApcArgument);
	}
	arch_disable_trap();
	cur->Flags &= ~PROCESS_FLAG_APCSTATE;
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
	ASSERT(TargetExecuteLevel < EXECUTE_LEVEL_ISR, BUG_BADLEVEL);
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	EXECUTE_LEVEL old = proc->ExecuteLevel;
	ASSERT(TargetExecuteLevel >= old, BUG_BADLEVEL);
	proc->ExecuteLevel = TargetExecuteLevel;
	// ObUnlockObject(proc);
	return old;
}

// The OldState must be the one returned by raise()!
// Otherwise, the kstack and trapframe would be corruptted! 
void KeLowerExecuteLevel(EXECUTE_LEVEL OriginalExecuteLevel)
{
	PKPROCESS proc = PsGetCurrentProcess();
	ASSERT(proc->ExecuteLevel >= OriginalExecuteLevel, BUG_BADLEVEL);
	ASSERT(OriginalExecuteLevel >= EXECUTE_LEVEL_RT || DpcWatchTimer[cpuid()] == -1, BUG_BADLEVEL);
	ASSERT(OriginalExecuteLevel >= EXECUTE_LEVEL_APC || !(proc->Flags & PROCESS_FLAG_APCSTATE), BUG_BADLEVEL);
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
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	int cid = cpuid();
	// Find the next
	// No need to lock the process, since only the scheduler can change its status.
	PKPROCESS cur = PsGetCurrentProcess();
	PKPROCESS nxt = container_of(cur->SchedulerList.Backward, KPROCESS, SchedulerList);
	// KeAcquireSpinLockFast(&ActiveListLock);
	if (WorkerSwitchTimer[cid] == 0)
	{
		WorkerSwitchTimer[cid] = WORKER_SWITCH_ROUND;
		if (cur != KernelProcess[cid] && nxt != KernelProcess[cid])
		{
			LibRemoveListEntry(&KernelProcess[cid]->SchedulerList);
			LibInsertListEntry(&cur->SchedulerList, &KernelProcess[cid]->SchedulerList);
			nxt = KernelProcess[cid];
		}
	}
	else if (WorkerSwitchTimer[cid] != -1)
		WorkerSwitchTimer[cid]--;
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
			KeBugFault(BUG_SCHEDULER);
			break;
	}
	switch (cur->Status)
	{
		case PROCESS_STATUS_RUNNING:
			cur->Status = PROCESS_STATUS_RUNABLE;
			break;
		case PROCESS_STATUS_ZOMBIE:
			LibRemoveListEntry(&cur->SchedulerList);
			ActiveProcessCount[cid]--;
			break;
		case PROCESS_STATUS_WAIT:
			LibRemoveListEntry(&cur->SchedulerList);
			ActiveProcessCount[cid]--;
			LibInsertListEntry(&InactiveList[cid], &cur->SchedulerList);
			break;
		default:
			KeBugFault(BUG_SCHEDULER);
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
}

RT_ONLY void PsiCheckInactiveList()
{
	if (KeBugFaultFlag) for(arch_disable_trap();;);
	int cid = cpuid();
	PLIST_ENTRY p = InactiveList[cid].Backward;
	for (;;)
	{
		PLIST_ENTRY np = p->Backward;
		if (p == &InactiveList[cid])
			break;
		PKPROCESS proc = container_of(p, KPROCESS, SchedulerList);
		if (proc->ApcList != NULL ||    // Alerted
			proc->WaitMutex == NULL)    // Signaled
		{
			proc->Status = PROCESS_STATUS_RUNABLE;
			LibRemoveListEntry(&proc->SchedulerList);
			LibInsertListEntry(&KernelProcess[cid]->SchedulerList, &proc->SchedulerList);
			ActiveProcessCount[cid]++;
		}
		p = np;
	}
}
