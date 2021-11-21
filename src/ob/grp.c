#include <ob/grp.h>
#include <def.h>

UNSAFE void PsiTaskSwitch(PKPROCESS);
void PgiWorkerEntry(PPROCESS_GROUP);

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

PPROCESS_GROUP PgCreateGroup()
{
	PPROCESS_GROUP g = (PPROCESS_GROUP)MmAllocateObject(&GroupPool);
	if (g == NULL)
		return NULL;
	PKPROCESS p = PsCreateProcessEx();
	if (p == NULL)
	{
		MmFreeObject(&GroupPool, g);
		return NULL;
	}
	g->Flags = 0;
	g->NextProcessId = 1;
	init_rc(&g->ReferenceCount);
	g->GroupWorker = p;
	g->SchedulerList.Forward = g->SchedulerList.Backward = &g->SchedulerList;
	p->Flags |= PROCESS_FLAG_GROUPWORKER | PROCESS_FLAG_KERNEL;
	p->ParentId = 0;
	p->GroupProcessId = 0;
	p->GroupProcessList.Forward = p->GroupProcessList.Backward = &p->GroupProcessList;
	p->Group = g;
	p->MemorySpace = NULL;
	memcpy(&p->DebugName, "group_worker", 13);
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	KeAcquireSpinLockFast(&GroupListLock);
	g->GroupId = NextGroupId++;
	LibInsertListEntry(&GroupList, &g->GroupList);
	KeReleaseSpinLockFast(&GroupListLock);
	PsCreateProcess(p, (PVOID)PgiWorkerEntry, (ULONG64)g);
	KeLowerExecuteLevel(oldel);
	return g;
}

void PgiDestroyGroup()
{
	PKPROCESS cur = PsGetCurrentProcess();
	ASSERT(cur->Flags & PROCESS_FLAG_GROUPWORKER, BUG_WORKER);
	cur->Group->Flags |= GROUP_FLAG_ZOMBIE;
}

void PgiStartNewProcess(PKPROCESS Process)
{
	printf("start %p, cur=%p, pg=%p, cg=%p\n", Process, PsGetCurrentProcess(), Process->Group, PsGetCurrentProcess()->Group);
	PPROCESS_GROUP g = PsGetCurrentProcess()->Group;
	ASSERT(Process->Group != g, BUG_SCHEDULER);
	Process->GroupProcessId = g->NextProcessId++;
	LibInsertListEntry(&g->GroupWorker->GroupProcessList, &Process->GroupProcessList);
	LibInsertListEntry(&g->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_RUNABLE;
}

void PgiAwakeProcess(PKPROCESS Process)
{
	PPROCESS_GROUP g = PsGetCurrentProcess()->Group;
	ASSERT(Process->Group != g, BUG_SCHEDULER);
	LibInsertListEntry(&g->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_RUNABLE;
}

void PgiWorkerEntry(PPROCESS_GROUP ProcessGroup)
{
	PKPROCESS cur = PsGetCurrentProcess();
	ASSERT(cur->ExecuteLevel == EXECUTE_LEVEL_USR, BUG_WORKER);
	ASSERT(cur->Group == ProcessGroup, BUG_WORKER);
	ASSERT(cur->Flags & PROCESS_FLAG_GROUPWORKER, BUG_WORKER);
	arch_disable_trap();
	PKPROCESS p = container_of(&ProcessGroup->SchedulerList, KPROCESS, SchedulerList);
	while (1)
	{
		printf("groupppppp\n");
		PKPROCESS np = container_of(p->SchedulerList.Backward, KPROCESS, SchedulerList);
		if (p != np)
		{
			if (&p->SchedulerList == &ProcessGroup->SchedulerList)
			{
				p = np;
				continue;
			}
			// If destroying the group...
			if (ProcessGroup->Flags & GROUP_FLAG_ZOMBIE)
			{
				PsTerminateProcess(p);
			}
			// switch to p
			PsiTaskSwitch(p);
			// update p
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
			p = np;
		}
		else
		{
			// TODO: Clean-up after destroyed
			// First we should determine the procedure of destroying a group...
		}
		KeTaskSwitch(); // also KeClearApcList()
	}
}