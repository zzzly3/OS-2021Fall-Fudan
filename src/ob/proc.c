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
	return TRUE;
}

PKPROCESS PsCreateProcessEx()
{
	PKPROCESS p = (PKPROCESS)MmAllocateObject(&ProcessPool);
	if (p == NULL)
		return NULL;
	p->Status = PROCESS_STATUS_INITIALIZE;
	KeInitializeSpinLock(p->Lock);
	PVOID g = MmAllocatePhysicalPage();
	if (g == NULL)
	{
		MmFreeObject(&ProcessPool, (PVOID)p);
		return NULL;
	}
	p->Context.KernelStack.p = (PVOID)((ULONG64)g + PAGE_SIZE - 64);
	KeAcquireSpinLock(&ProcessListLock);
	p->ProcessId = NextProcessId++;
	KeReleaseSpinLock(&ProcessListLock);
	return p;
}

