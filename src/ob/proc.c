#include <ob/proc.h>

static OBJECT_POOL ProcessPool;
static SPINLOCK ProcessListLock[CPU_NUM];
int NextProcessId[CPU_NUM];
PKPROCESS KernelProcess[CPU_NUM];

void PsUserProcessEntry();
void PsKernelProcessEntry();
RT_ONLY void PsiStartNewProcess(PKPROCESS);
void PsiExitProcess();

BOOL ObInitializeProcessManager()
{
	int cid = cpuid();
	if (cid == START_CPU)
		MmInitializeObjectPool(&ProcessPool, sizeof(KPROCESS));
	KeInitializeSpinLock(&ProcessListLock[cid]);
	KernelProcess[cid] = PsCreateProcessEx();
	if (KernelProcess[cid] == NULL)
		return FALSE;
	KernelProcess[cid]->ExecuteLevel = EXECUTE_LEVEL_RT;
	KernelProcess[cid]->Flags |= PROCESS_FLAG_KERNEL;
	KernelProcess[cid]->ProcessList.Forward = KernelProcess[cid]->ProcessList.Backward = &KernelProcess[cid]->ProcessList;
	KernelProcess[cid]->SchedulerList.Forward = KernelProcess[cid]->SchedulerList.Backward = &KernelProcess[cid]->SchedulerList;
	KernelProcess[cid]->WaitList.Forward = KernelProcess[cid]->WaitList.Backward = &KernelProcess[cid]->WaitList;
	ObInitializeScheduler();
	return TRUE;
}

PKPROCESS PsCreateProcessEx()
{
	int cid = cpuid();
	BOOL trapen = arch_disable_trap();
	PKPROCESS p = (PKPROCESS)MmAllocateObject(&ProcessPool);
	if (p == NULL)
		return NULL;
	p->Status = PROCESS_STATUS_INITIALIZE;
	p->ExecuteLevel = EXECUTE_LEVEL_USR;
	p->ApcList = NULL;
	p->WaitMutex = NULL;
	KeInitializeSpinLock(&p->Lock);
	PVOID g = MmAllocatePhysicalPage();
	if (g == NULL)
	{
		MmFreeObject(&ProcessPool, (PVOID)p);
		return NULL;
	}
	p->Context.KernelStack.p = (PVOID)((ULONG64)g + PAGE_SIZE - 192);
	KeAcquireSpinLockFast(&ProcessListLock[cid]);
	p->ProcessId = (NextProcessId[cid]++) * CPU_NUM + cid;
	KeReleaseSpinLockFast(&ProcessListLock[cid]);
	if (trapen)
		arch_enable_trap();
	return p;
}

// Create & run the process described by the object
RT_ONLY void PsCreateProcess(PKPROCESS Process, PVOID ProcessEntry, ULONG64 EntryArgument)
{
	int cid = cpuid();
	if (Process->Flags & PROCESS_FLAG_KERNEL) // kernel process
	{
		Process->Context.KernelStack.d->lr = (ULONG64)PsKernelProcessEntry;
		Process->Context.KernelStack.d->x0 = (ULONG64)ProcessEntry;
		Process->Context.KernelStack.d->x1 = EntryArgument;
	}
	else
	{
		Process->Context.KernelStack.d->lr = (ULONG64)PsUserProcessEntry;
		Process->Context.KernelStack.d->x0 = (ULONG64)ProcessEntry;
		Process->Context.KernelStack.d->x1 = EntryArgument;
	}
	KeAcquireSpinLockFast(&ProcessListLock[cid]);
	LibInsertListEntry(&KernelProcess[cid]->ProcessList, &Process->ProcessList);
	KeReleaseSpinLockFast(&ProcessListLock[cid]);
	PsiStartNewProcess(Process);
}

KSTATUS KeCreateProcess(PKSTRING ProcessName, PVOID ProcessEntry, ULONG64 EntryArgument, int* ProcessId)
{
	PKPROCESS p = PsCreateProcessEx();
	if (p == NULL)
		return STATUS_NO_ENOUGH_MEMORY;
	p->Flags |= PROCESS_FLAG_KERNEL;
	if (ProcessName != NULL)
		LibKStringToCString(ProcessName, p->DebugName, 16);
	p->ParentId = KernelProcess[cpuid()]->ProcessId;
	p->MemorySpace = NULL;
	*ProcessId = p->ProcessId;
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	PsCreateProcess(p, ProcessEntry, EntryArgument);
	KeLowerExecuteLevel(oldel);
	return STATUS_SUCCESS;
}

void KeExitProcess()
{
	// KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	PsiExitProcess();
}
