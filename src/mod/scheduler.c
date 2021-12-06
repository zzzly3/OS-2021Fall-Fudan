#include <mod/scheduler.h>
#include <def.h>

extern BOOL KeBugFaultFlag;
extern PKPROCESS KernelProcess[CPU_NUM];
// Although one core has only one active list, locks are no longer needed.
// But RT level is still needed to avoid being interrupted.
// static SPINLOCK ActiveListLock;
// static SPINLOCK InactiveListLock;
int WorkerSwitchTimer[CPU_NUM];
int DpcWatchTimer[CPU_NUM];
PDPC_ENTRY DpcList;
static SPINLOCK DpcListLock;
static OBJECT_POOL ApcObjectPool, DpcObjectPool;
int ActiveProcessCount[CPU_NUM];
int WakenProcessCount;
PKPROCESS TransferProcess;
static SPINLOCK TransferListLock;
static SPINLOCK WakenListLock;
LIST_ENTRY WakenList;
// int WaitingProcessCount[CPU_NUM];
// PKPROCESS TransferWaitingProcess;
// static LIST_ENTRY InactiveList[CPU_NUM];
// static SPINLOCK TransferWaitingListLock;

void swtch (PVOID kstack, PVOID* oldkstack);
void KiClockTrapEntry();
void PgiAwakeProcess(PKPROCESS);

void ObInitializeScheduler()
{
	int cid = cpuid();
	// KeInitializeSpinLock(&ActiveListLock);
	if (cid == START_CPU)
	{
		KeInitializeSpinLock(&DpcListLock);
		KeInitializeSpinLock(&TransferListLock);
		KeInitializeSpinLock(&WakenListLock);
		// KeInitializeSpinLock(&TransferWaitingListLock);
		MmInitializeObjectPool(&ApcObjectPool, sizeof(APC_ENTRY));
		MmInitializeObjectPool(&DpcObjectPool, sizeof(DPC_ENTRY));
		DpcList = NULL;
		TransferProcess = NULL;
		WakenList.Backward = WakenList.Forward = &WakenList;
		// TransferWaitingProcess = NULL;
	}
	WorkerSwitchTimer[cid] = -1;
	ActiveProcessCount[cid] = 1;
	// WaitingProcessCount[cid] = 0;
	WakenProcessCount = 0;
	DpcWatchTimer[cid] = -1;
	// InactiveList[cid].Forward = InactiveList[cid].Backward = &InactiveList[cid];
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

RT_ONLY void PsiTransferProcess()
{
	// No need for precision
	// Active process
	int maxn = 0, minn = ~(1 << 31), cid = cpuid(), mn = ActiveProcessCount[cid];
	for (int i = 0; i < CPU_NUM; i++)
		maxn = max(maxn, ActiveProcessCount[i]), minn = min(minn, ActiveProcessCount[i]);
	if (maxn > minn + 1 && mn == maxn && mn > 2 && TransferProcess == NULL)
	{
		// Push out
		KeAcquireSpinLockFast(&TransferListLock);
		if (TransferProcess == NULL)
		{
			// Choose the last one
			PKPROCESS p = container_of(KernelProcess[cid]->SchedulerList.Forward, KPROCESS, SchedulerList);
			// TODO: Upgrade the policy
			if (!(p->Flags & PROCESS_FLAG_BINDCPU))
			{
				LibRemoveListEntry(&p->SchedulerList);
				ActiveProcessCount[cid]--;
				TransferProcess = p;
			}
		}
		KeReleaseSpinLockFast(&TransferListLock);
	}
	if (mn == minn && TransferProcess != NULL)
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
	// Accept waken process
	if (WakenList.Backward != &WakenList)
	{
		KeAcquireSpinLockFast(&WakenListLock);
		// NOTE: Magic numberrrrrrrr
		PLIST_ENTRY insert_node = &KernelProcess[cid]->SchedulerList;
		for (int i = 0; i < WORKER_SWITCH_ROUND / 2 + 1; i++)
		{
			PLIST_ENTRY pe = WakenList.Backward;
			if (pe == &WakenList)
				break;
			LibRemoveListEntry(pe);
			LibInsertListEntry(insert_node, pe);
			container_of(pe, KPROCESS, SchedulerList)->Status = PROCESS_STATUS_RUNABLE;
			ActiveProcessCount[cid]++;
			WakenProcessCount--;
		}
		KeReleaseSpinLockFast(&WakenListLock);
	}
}

UNSAFE void PsiAwakeProcess(PKPROCESS Process)
{
	if (Process->Flags & PROCESS_FLAG_WAITING)
	{
		Process->Flags &= ~PROCESS_FLAG_WAITING;
		PKPROCESS gk = PgGetProcessGroupWorker(Process);
		if (gk == NULL)
		{
			KeAcquireSpinLockFast(&WakenListLock);
			LibInsertListEntry(WakenList.Forward, &Process->SchedulerList);
			WakenProcessCount++;
			KeReleaseSpinLockFast(&WakenListLock);
		}
		else
		{
			KeCreateApcEx(gk, (PAPC_ROUTINE)PgiAwakeProcess, (ULONG64)Process);
		}
	}
}

void PsAlertProcess(PKPROCESS Process)
{
	ObLockObject(Process);
	PsiAwakeProcess(Process);
	ObUnlockObject(Process);
}

void PsTerminateProcess(PKPROCESS Process)
{
	KeCreateApcEx(Process, (PAPC_ROUTINE)PsiExitProcess, NULL);
}

void KiClockTrapEntry()
{
	int cid = cpuid();
	if (KeBugFaultFlag) for(arch_disable_trap();;);
	do // NOTE: Debug HELLO
	{
		static int dbgcd[CPU_NUM];
		if (dbgcd[cid] == 1000 / TIME_SLICE_MS)
		{
			printf("CPU %d HELLO!\n", cid);
			dbgcd[cid] = 0;
		}
		else
			dbgcd[cid]++;
	} while (0);
	reset_clock(TIME_SLICE_MS);
	//uart_put_char('t');
	PKPROCESS cur = PsGetCurrentProcess();
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

BOOL KeCreateDpc(PDPC_ROUTINE Routine, ULONG64 Argument)
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
	return p ? TRUE : FALSE;
}

BOOL KeCreateApcEx(PKPROCESS Process, PAPC_ROUTINE Routine, ULONG64 Argument)
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
		PsiAwakeProcess(Process);
		ObUnlockObjectFast(Process);
	}
	if (trapen)
		arch_enable_trap();
	return p ? TRUE : FALSE;
}

BOOL KeQueueWorkerApc(PAPC_ROUTINE Routine, ULONG64 Argument)
{
	static int which;
	which = (which + 1) % CPU_NUM;
	return KeCreateApcEx(KernelProcess[which], Routine, Argument);
}

UNSAFE void KeCancelApcs(PKPROCESS Process)
{
	ObLockObjectFast(Process);
	PAPC_ENTRY p = Process->ApcList;
	while (p)
	{
		PAPC_ENTRY np = p->NextEntry;
		// TODO: Cancel callback?
		MmFreeObject(&ApcObjectPool, p);
		p = np;
	}
	ObUnlockObjectFast(Process);
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

// Lock-free: only the scheduler can call
UNSAFE void PsiTaskSwitch(PKPROCESS NextTask)
{
	PKPROCESS cur = PsGetCurrentProcess();
	// printf("*%d->%d\n", cur->ProcessId, NextTask->ProcessId);
	PKPROCESS gw = PgGetProcessGroupWorker(cur);
	ASSERT((NextTask->Flags & PROCESS_FLAG_WAITING) == 0, BUG_SCHEDULER);
	// NextTask->Status = PROCESS_STATUS_RUNNING;
	ASSERT(NextTask->Status == PROCESS_STATUS_RUNNING, BUG_SCHEDULER);
	cur->Context.UserStack = (PVOID)arch_get_usp();
	arch_set_usp((ULONG64)NextTask->Context.UserStack);
	MmSwitchMemorySpaceEx(cur->MemorySpace, NextTask->MemorySpace);
	arch_set_tid((ULONG64)NextTask);
	swtch(NextTask->Context.KernelStack.p, &cur->Context.KernelStack.p);
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	KeLowerExecuteLevel(oldel);
}

UNSAFE void KeTaskSwitch()
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	int cid = cpuid();
	// Find the next
	// No need to lock the process, since only the scheduler can change its status.
	PKPROCESS cur = PsGetCurrentProcess();
	PKPROCESS nxt = container_of(cur->SchedulerList.Backward, KPROCESS, SchedulerList);
	PKPROCESS gw = PgGetCurrentGroupWorker();
	if (gw == NULL)
	{
		// root scheduler
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
			return;
		}
		// Do switch
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
				ObLockObjectFast(cur);
				if (cur->WaitMutex == NULL || cur->ApcList != NULL)
				{
					// Awake immediately
					cur->Status = PROCESS_STATUS_RUNABLE;
				}
				else
				{
					LibRemoveListEntry(&cur->SchedulerList);
					cur->Flags |= PROCESS_FLAG_WAITING;
					ActiveProcessCount[cid]--;
				}
				ObUnlockObjectFast(cur);
				break;
			default:
				KeBugFault(BUG_SCHEDULER);
				break;
		}
		ASSERT(nxt->Status == PROCESS_STATUS_RUNABLE, BUG_SCHEDULER);
		nxt->Status = PROCESS_STATUS_RUNNING;
	}
	else
	{
		// group scheduler
		nxt = gw;
	}
	PsiTaskSwitch(nxt);
}

// RT_ONLY void PsiCheckInactiveList()
// {
// 	if (KeBugFaultFlag) for(arch_disable_trap();;);
// 	int cid = cpuid();
// 	PLIST_ENTRY p = InactiveList[cid].Backward;
// 	for (;;)
// 	{
// 		PLIST_ENTRY np = p->Backward;
// 		if (p == &InactiveList[cid])
// 			break;
// 		PKPROCESS proc = container_of(p, KPROCESS, SchedulerList);
// 		if (proc->ApcList != NULL ||    // Alerted
// 			proc->WaitMutex == NULL)    // Signaled
// 		{
// 			proc->Status = PROCESS_STATUS_RUNABLE;
// 			LibRemoveListEntry(&proc->SchedulerList);
// 			LibInsertListEntry(&KernelProcess[cid]->SchedulerList, &proc->SchedulerList);
// 			ActiveProcessCount[cid]++;
// 			WaitingProcessCount[cid]--;
// 		}
// 		p = np;
// 	}
// }
