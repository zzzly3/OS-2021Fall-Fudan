#include <ob/proc.h>

static OBJECT_POOL ProcessPool;
static SPINLOCK ProcessListLock;
int NextProcessId;
PKPROCESS KernelProcess;

BOOL ObInitializeProcessManager()
{
	MmInitializeObjectPool(&ProcessPool, sizeof(KPROCESS));
	KeInitializeSpinLock(&ProcessListLock);
	KernelProcess = PsCreateProcessEx();
	if (KernelProcess == NULL)
		return FALSE;
	KernelProcess->Flags |= PROCESS_FLAG_KERNEL;
	KernelProcess->ProcessList.Forward = KernelProcess->ProcessList.Backward = &KernelProcess->ProcessList;
	return TRUE;
}

PKPROCESS PsCreateProcessEx()
{
	PKPROCESS p = (PKPROCESS)MmAllocateObject(&ProcessPool);
	if (p == NULL)
		return NULL;
	p->Status = PROCESS_STATUS_INITIALIZE;
	KeInitializeSpinLock(&p->Lock);
	PVOID g = MmAllocatePhysicalPage();
	if (g == NULL)
	{
		MmFreeObject(&ProcessPool, (PVOID)p);
		return NULL;
	}
	p->Context.KernelStack.p = (PVOID)((ULONG64)g + PAGE_SIZE - 192);
	KeAcquireSpinLock(&ProcessListLock);
	p->ProcessId = NextProcessId++;
	KeReleaseSpinLock(&ProcessListLock);
	return p;
}

// Create & run the process described by the object
void proc_entry();
void PsCreateProcess(PKPROCESS Process, ULONG64 UserEntry, ULONG64 UserArgument)
{
	Process->Context.KernelStack.d->lr = (ULONG64)proc_entry;
	Process->Context.KernelStack.d->x0 = UserEntry;
	Process->Context.KernelStack.d->x1 = UserArgument;
	KeAcquireSpinLock(&ProcessListLock);
	LibInsertListEntry(&KernelProcess->ProcessList, &Process->ProcessList);
	KeReleaseSpinLock(&ProcessListLock);
}

PKPROCESS PsGetCurrentProcess()
{
	// TODO
	return NULL;
}
