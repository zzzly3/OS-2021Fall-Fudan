#include <ob/grp.h>
#include <def.h>

UNSAFE void PsiTaskSwitch(PKPROCESS);
void PgiWorkerEntry(PPROCESS_GROUP);
void PsiFreeProcess(PKPROCESS);

static OBJECT_POOL GroupPool;
static SPINLOCK GroupListLock;
static LIST_ENTRY GroupList;
int NextGroupId;

void ObInitializeGroupManager()
{
	KeInitializeSpinLock(&GroupListLock);
	GroupList.Backward = GroupList.Forward = &GroupList;
	MmInitializeObjectPool(&GroupPool, sizeof(PROCESS_GROUP));
	NextGroupId = 1;
}

PPROCESS_GROUP PgCreateGroup(int WorkerCount)
{
	ASSERT(WorkerCount > 0 && WorkerCount <= CPU_NUM, BUG_WORKER);
	BOOL te = arch_disable_trap();
	PPROCESS_GROUP g = (PPROCESS_GROUP)MmAllocateObject(&GroupPool);
	if (g == NULL)
		goto fail;
	g->Flags = 0;
	g->NextProcessId = 1;
	KeInitializeSpinLock(&g->Lock);
	init_rc(&g->ReferenceCount);
	increment_rc(&g->ReferenceCount);
	g->NextRun = container_of(&g->SchedulerList, KPROCESS, SchedulerList);
	g->SchedulerList.Forward = g->SchedulerList.Backward = &g->SchedulerList;
	PKPROCESS p[CPU_NUM] = {NULL};
	for (int i = 0; i < WorkerCount; i++)
	{
		p[i] = PsCreateProcessEx();
		if (p[i] == NULL)
			goto fail;
		p[i]->Flags |= PROCESS_FLAG_GROUPWORKER | PROCESS_FLAG_KERNEL;
		p[i]->ParentId = 0;
		p[i]->GroupProcessId = 0;
		p[i]->GroupProcessList.Forward = p[i]->GroupProcessList.Backward = &p[i]->GroupProcessList;
		p[i]->Group = g;
		p[i]->MemorySpace = NULL;
		memcpy(&p[i]->DebugName, "group_worker", 13);
		g->GroupWorker[i] = p[i];
	}
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	KeAcquireSpinLockFast(&GroupListLock);
	g->GroupId = NextGroupId++;
	LibInsertListEntry(&GroupList, &g->GroupList);
	KeReleaseSpinLockFast(&GroupListLock);
	for (int i = 0; i < WorkerCount; i++)
		PsCreateProcess(p[i], (PVOID)PgiWorkerEntry, (ULONG64)g);
	KeLowerExecuteLevel(oldel);
	if (te) arch_enable_trap();
	return g;
fail:
	if (g != NULL)
		MmFreeObject(&GroupPool, g);
	for (int i = 0; i < WorkerCount; i++)
	{
		if (p[i] != NULL)
			PsiFreeProcess(p[i]);
	}
	if (te) arch_enable_trap();
}

// TODO: PgDestroyGroup()

void PgiDestroyGroup()
{
	PKPROCESS cur = PsGetCurrentProcess();
	ASSERT(cur->Flags & PROCESS_FLAG_GROUPWORKER, BUG_WORKER);
	cur->Group->Flags |= GROUP_FLAG_ZOMBIE;
}

void PgiStartNewProcess(PKPROCESS Process)
{
	PPROCESS_GROUP g = PsGetCurrentProcess()->Group;
	ASSERT(Process->Group == g, BUG_SCHEDULER);
	Process->GroupProcessId = g->NextProcessId++;
	LibInsertListEntry(&g->GroupWorker[0]->GroupProcessList, &Process->GroupProcessList);
	ObLockObject(g);
	LibInsertListEntry(&g->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_RUNABLE;
	ObUnlockObject(g);
}

void PgiAwakeProcess(PKPROCESS Process)
{
	PPROCESS_GROUP g = PsGetCurrentProcess()->Group;
	ASSERT(Process->Group == g, BUG_SCHEDULER);
	ObLockObject(g);
	LibInsertListEntry(&g->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_RUNABLE;
	ObUnlockObject(g);
}

void PgiWorkerEntry(PPROCESS_GROUP ProcessGroup)
{
	PKPROCESS cur = PsGetCurrentProcess();
	ASSERT(cur->ExecuteLevel == EXECUTE_LEVEL_USR, BUG_WORKER);
	ASSERT(cur->Group == ProcessGroup, BUG_WORKER);
	ASSERT(cur->Flags & PROCESS_FLAG_GROUPWORKER, BUG_WORKER);
	arch_disable_trap();
	while (1)
	{
		ObLockObjectFast(ProcessGroup);
		PKPROCESS p;
		int skip = 0;
	get_next:
		p = ProcessGroup->NextRun;
		ProcessGroup->NextRun = container_of(p->SchedulerList.Backward, KPROCESS, SchedulerList);
		if (p == ProcessGroup->NextRun)
			goto end_sched;
		if (&p->SchedulerList == &ProcessGroup->SchedulerList)
			goto get_next;
		if (p->Status == PROCESS_STATUS_RUNABLE)
		{
			// If destroying the group...
			if (ProcessGroup->Flags & GROUP_FLAG_ZOMBIE)
			{
				PsTerminateProcess(p);
			}
			// switch to p
			p->Status = PROCESS_STATUS_RUNNING;
			ObUnlockObjectFast(ProcessGroup);
			PsiTaskSwitch(p);
			// update p
			ObLockObjectFast(ProcessGroup);
			switch (p->Status)
			{
				case PROCESS_STATUS_RUNNING:
					p->Status = PROCESS_STATUS_RUNABLE;
					break;
				case PROCESS_STATUS_ZOMBIE:
					// exit
					LibRemoveListEntry(&p->SchedulerList);
					break;
				case PROCESS_STATUS_WAIT:
					ObLockObjectFast(p);
					if (p->WaitMutex == NULL || p->ApcList != NULL)
					{
						// Awake immediately
						p->Status = PROCESS_STATUS_RUNABLE;
					}
					else
					{
						LibRemoveListEntry(&p->SchedulerList);
						p->Flags |= PROCESS_FLAG_WAITING;
					}
					ObUnlockObjectFast(p);
					break;
				default:
					KeBugFault(BUG_WORKER);
					break;
			}
		}
		else
		{
			ASSERT(p->Status == PROCESS_STATUS_RUNNING, BUG_WORKER);
			if (skip < 1)
			{
				skip++;
				goto get_next;
			}
		}
	end_sched:
		ObUnlockObjectFast(ProcessGroup);
		KeTaskSwitch();
	}
}

PKPROCESS PgGetProcessGroupWorker(PKPROCESS Process)
{
	if ((Process->Flags & PROCESS_FLAG_GROUPWORKER) == 0 && Process->Group != NULL)
	{
		return Process->Group->GroupWorker[0];
	}
	return NULL;
}