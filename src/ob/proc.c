#include <ob/proc.h>

static OBJECT_POOL ProcessPool;
static SPINLOCK ProcessListLock;
int NextProcessId;
PKPROCESS KernelProcess;

void PsUserProcessEntry();
void PsKernelProcessEntry();
void PsiStartNewProcess(PKPROCESS);

BOOL ObInitializeProcessManager()
{
	MmInitializeObjectPool(&ProcessPool, sizeof(KPROCESS));
	KeInitializeSpinLock(&ProcessListLock);
	KernelProcess = PsCreateProcessEx();
	if (KernelProcess == NULL)
		return FALSE;
	KernelProcess->ExecuteLevel = EXECUTE_LEVEL_RT;
	KernelProcess->Flags |= PROCESS_FLAG_KERNEL;
	KernelProcess->ProcessList.Forward = KernelProcess->ProcessList.Backward = &KernelProcess->ProcessList;
	KernelProcess->SchedulerList.Forward = KernelProcess->SchedulerList.Backward = &KernelProcess->SchedulerList;
	ObInitializeScheduler();
	return TRUE;
}

PKPROCESS PsCreateProcessEx()
{
	BOOL trapen = arch_disable_trap();
	PKPROCESS p = (PKPROCESS)MmAllocateObject(&ProcessPool);
	if (p == NULL)
		return NULL;
	p->Status = PROCESS_STATUS_INITIALIZE;
	p->ExecuteLevel = EXECUTE_LEVEL_USR;
	p->ApcList = NULL;
	KeInitializeSpinLock(&p->Lock);
	PVOID g = MmAllocatePhysicalPage();
	if (g == NULL)
	{
		MmFreeObject(&ProcessPool, (PVOID)p);
		return NULL;
	}
	p->Context.KernelStack.p = (PVOID)((ULONG64)g + PAGE_SIZE - 192);
	KeAcquireSpinLockFast(&ProcessListLock);
	p->ProcessId = NextProcessId++;
	KeReleaseSpinLockFast(&ProcessListLock);
	if (trapen)
		arch_enable_trap();
	return p;
}

// Create & run the process described by the object
RT_ONLY void PsCreateProcess(PKPROCESS Process, PVOID ProcessEntry, ULONG64 EntryArgument)
{
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
	KeAcquireSpinLockFast(&ProcessListLock);
	LibInsertListEntry(&KernelProcess->ProcessList, &Process->ProcessList);
	KeReleaseSpinLockFast(&ProcessListLock);
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
	p->ParentId = KernelProcess->ProcessId;
	p->MemorySpace = NULL;
	*ProcessId = p->ProcessId;
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	PsCreateProcess(p, ProcessEntry, EntryArgument);
	KeLowerExecuteLevel(oldel);
	return STATUS_SUCCESS;
}
