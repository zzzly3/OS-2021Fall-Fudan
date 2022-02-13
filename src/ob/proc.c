#include <ob/proc.h>
#include <def.h>
#include <fs/cache.h>
#include <mod/bug.h>
#include <fs/file.h>

static OBJECT_POOL ProcessPool;
static SPINLOCK ProcessListLock;
int NextProcessId;
PKPROCESS KernelProcess[CPU_NUM];

void PsUserProcessEntry();
void PsKernelProcessEntry();
void PsUserForkEntry();
RT_ONLY void PsiStartNewProcess(PKPROCESS);
void PsiExitProcess();
void PgiStartNewProcess(PKPROCESS);

#define next_process(proc) container_of((proc)->ProcessList.Backward, KPROCESS, ProcessList)

void ObInitializeMessages();
BOOL ObInitializeProcessManager()
{
	int cid = cpuid();
	if (cid == START_CPU)
	{
		MmInitializeObjectPool(&ProcessPool, sizeof(KPROCESS));
		KeInitializeSpinLock(&ProcessListLock);
		ObInitializeGroupManager();
		ObInitializeMessages();
	}
	KernelProcess[cid] = PsCreateProcessEx();
	if (KernelProcess[cid] == NULL)
		return FALSE;
	KernelProcess[cid]->ExecuteLevel = EXECUTE_LEVEL_RT;
	KernelProcess[cid]->Flags |= PROCESS_FLAG_KERNEL | PROCESS_FLAG_BINDCPU;
	KernelProcess[cid]->SchedulerList.Forward = KernelProcess[cid]->SchedulerList.Backward = &KernelProcess[cid]->SchedulerList;
	KernelProcess[cid]->WaitList.Forward = KernelProcess[cid]->WaitList.Backward = &KernelProcess[cid]->WaitList;
	KeAcquireSpinLockFast(&ProcessListLock);
	if (cid == START_CPU)
	{
		KernelProcess[cid]->ProcessList.Forward = KernelProcess[cid]->ProcessList.Backward = &KernelProcess[cid]->ProcessList;
	}
	else
	{
		LibInsertListEntry(&KernelProcess[START_CPU]->ProcessList, &KernelProcess[cid]->ProcessList);
	}
	KeReleaseSpinLockFast(&ProcessListLock);
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
	p->Group = NULL;
	p->GroupWorker = NULL;
	p->GroupProcessList.Forward = p->GroupProcessList.Backward = NULL;
	KeInitializeSpinLock(&p->Lock);
	KeInitializeMessageQueue(&p->MessageQueue);
	init_rc(&p->ReferenceCount);
	increment_rc(&p->ReferenceCount);
	PVOID g = MmAllocatePhysicalPage();
	if (g == NULL)
	{
		MmFreeObject(&ProcessPool, (PVOID)p);
		return NULL;
	}
	p->Context.KernelStack.p = (PVOID)((ULONG64)g + PAGE_SIZE - 192);
	KeAcquireSpinLockFast(&ProcessListLock);
	p->ProcessId = (NextProcessId++) * CPU_NUM + cid;
	KeReleaseSpinLockFast(&ProcessListLock);
	if (trapen)
		arch_enable_trap();
	return p;
}

// Create & run the process described by the object
RT_ONLY void PsCreateProcess(PKPROCESS Process, PVOID ProcessEntry, ULONG64 EntryArgument)
{
	ASSERT(PsGetCurrentProcess()->ExecuteLevel == EXECUTE_LEVEL_RT, BUG_BADLEVEL);
	if (Process->Flags & PROCESS_FLAG_KERNEL) // kernel process
	{
		Process->Context.KernelStack.d->lr = (ULONG64)PsKernelProcessEntry;
		Process->Context.KernelStack.d->x0 = (ULONG64)ProcessEntry;
		Process->Context.KernelStack.d->x1 = EntryArgument;
	}
	else
	{
		if (Process->Flags & PROCESS_FLAG_FORK) // forkret
		{
			Process->Context.KernelStack.d->lr = (ULONG64)PsUserForkEntry;
			Process->Context.KernelStack.d->x0 = EntryArgument;
		}
		else // user
		{
			Process->Context.KernelStack.d->lr = (ULONG64)PsUserProcessEntry;
			Process->Context.KernelStack.d->x0 = (ULONG64)ProcessEntry;
			Process->Context.KernelStack.d->x1 = EntryArgument;
		}
	}
	KeAcquireSpinLockFast(&ProcessListLock);
	LibInsertListEntry(&KernelProcess[0]->ProcessList, &Process->ProcessList);
	KeReleaseSpinLockFast(&ProcessListLock);
	PKPROCESS gk = PgGetProcessGroupWorker(Process);
	//printf("create %p use %p\n", Process, gk);
	if (gk)
	{
		// start with group scheduler
		KeCreateApcEx(gk, (PAPC_ROUTINE)PgiStartNewProcess, (ULONG64)Process);
	}
	else
	{
		// start with root scheduler
		PsiStartNewProcess(Process);
	}
}

KSTATUS KeCreateProcess(PKSTRING ProcessName, PVOID ProcessEntry, ULONG64 EntryArgument, int* ProcessId)
{
	PKPROCESS p = PsCreateProcessEx();
	if (p == NULL)
		return STATUS_NO_ENOUGH_MEMORY;
	p->Flags |= PROCESS_FLAG_KERNEL;
	if (ProcessName != NULL)
		LibKStringToCString(ProcessName, p->DebugName, 16);
	p->Parent = PsGetCurrentProcess();
	p->MemorySpace = NULL;
	*ProcessId = p->ProcessId;
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	PsCreateProcess(p, ProcessEntry, EntryArgument);
	KeLowerExecuteLevel(oldel);
	ObDereferenceObject(p);
	return STATUS_SUCCESS;
}

void KeExitProcess()
{
	// KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	printf("exit\n");
	PsiExitProcess();
}

void PsiFreeProcess(PKPROCESS Process)
{
	MmFreeObject(&ProcessPool, Process);
}

static OpContext SingleOpCtx;
void PsFreeProcess(PKPROCESS Process)
{
	// TODO: clean ref?
	ASSERT(Process->ReferenceCount.count == 1, BUG_BADREF);
	ASSERT(Process->Status == PROCESS_STATUS_ZOMBIE, BUG_SCHEDULER);
	ASSERT(Process->WaitMutex == NULL, BUG_SCHEDULER);
	BOOL te = arch_disable_trap();
	// notify
	ObLockObjectFast(Process);
	if (Process->Parent)
	{
		KeQueueMessage(&Process->Parent->MessageQueue, MSG_TYPE_CHILDEXIT, PsGetProcessId(Process));
		Process->Parent = NULL;
	}
	ObUnlockObjectFast(Process);
	// remove
	KeAcquireSpinLockFast(&ProcessListLock);
	PKPROCESS p = next_process(Process);
	while (p != Process)
	{
		if (p->Parent == Process)
		{
			ObLockObjectFast(p);
			p->Parent = NULL;
			ObUnlockObjectFast(p);
		}
		p = next_process(p);
	}
	LibRemoveListEntry(&Process->ProcessList);
	KeReleaseSpinLockFast(&ProcessListLock);
	// Free resources
	KeCancelApcs(Process);
	KeClearMessageQueue(&Process->MessageQueue);
	if (Process->MemorySpace)
		MmDestroyMemorySpace(Process->MemorySpace);
	if (Process->Cwd)
	{
		bcache.begin_op(&SingleOpCtx);
		inodes.put(&SingleOpCtx, Process->Cwd);
		bcache.end_op(&SingleOpCtx);
	}
	for (int i = 0; i < 16; i++)
	{
		if (Process->FileDescriptors[i])
		{
			fileclose(ifile(Process->FileDescriptors[i]));
		}
	}
	// Free process object
	MmFreeObject(&ProcessPool, Process);
	if (te) arch_enable_trap();
}

KSTATUS PsReferenceProcessById(int ProcessId, PKPROCESS* Process)
{
	KSTATUS ret = STATUS_OBJECT_NOT_FOUND;
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	KeAcquireSpinLockFast(&ProcessListLock);
	PKPROCESS p = next_process(KernelProcess[0]);
	while (p != KernelProcess[0])
	{
		if (p->ProcessId == ProcessId)
		{
			if (p->Status != PROCESS_STATUS_INITIALIZE &&
				p->Status != PROCESS_STATUS_ZOMBIE)
			{
				if (ObReferenceObject(p))
				{
					ret = STATUS_SUCCESS;
					*Process = p;
				}
				else
					ret = STATUS_MAX_REFERENCE;
			}
			break;
		}
		p = next_process(p);
	}
	KeReleaseSpinLockFast(&ProcessListLock);
	KeLowerExecuteLevel(oldel);
	return ret;
}

PMEMORY_SPACE KeSwitchMemorySpace(PMEMORY_SPACE MemorySpace)
{
	BOOL te = arch_disable_trap();
	PKPROCESS cur = PsGetCurrentProcess();
	PMEMORY_SPACE old = cur->MemorySpace;
	MmSwitchMemorySpaceEx(old, MemorySpace);
	cur->MemorySpace = MemorySpace;
	if (te) arch_enable_trap();
	return old;
}

KSTATUS KeCreateUserPage(PVOID VirtualAddress)
{
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->MemorySpace == NULL)
		return STATUS_NO_MEMORY_SPACE;
	BOOL te = arch_disable_trap();
	KSTATUS ret = MmCreateUserPageEx(cur->MemorySpace, VirtualAddress);
	if (te) arch_disable_trap();
	return ret;
}

KSTATUS KeMapUserPage(PVOID VirtualAddress, ULONG64 PageDescriptor)
{
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->MemorySpace == NULL)
		return STATUS_NO_MEMORY_SPACE;
	BOOL te = arch_disable_trap();
	KSTATUS ret = MmMapPageEx(cur->MemorySpace, VirtualAddress, PageDescriptor);
	if (te) arch_disable_trap();
	return ret;
}

KSTATUS KeUnmapUserPage(PVOID VirtualAddress)
{
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->MemorySpace == NULL)
		return STATUS_NO_MEMORY_SPACE;
	BOOL te = arch_disable_trap();
	KSTATUS ret = MmUnmapPageEx(cur->MemorySpace, VirtualAddress);
	if (te) arch_disable_trap();
	return ret;
}