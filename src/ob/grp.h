#ifndef __GRP_H__
#define __GRP_H__

#include <ob/proc.h>
#include <common/rc.h>
#include <mod/scheduler.h>

#define GROUP_FLAG_ZOMBIE 1

typedef struct _PROCESS_GROUP
{
	SPINLOCK Lock;
	REF_COUNT ReferenceCount; // NOTE: References are read-only
	int GroupId;
	int Flags;
	int WorkerCount;
	int NextProcessId;
	struct _KPROCESS* NextRun;
	struct _KPROCESS* GroupWorker[CPU_NUM]; // WARNING: NEVER block the worker!!!
	LIST_ENTRY GroupList;
	LIST_ENTRY SchedulerList;
} PROCESS_GROUP, *PPROCESS_GROUP;

void ObInitializeGroupManager();
PPROCESS_GROUP PgCreateGroup();
struct _KPROCESS* PgGetProcessGroupWorker(struct _KPROCESS*);
#define PgGetCurrentGroupWorker() PgGetProcessGroupWorker(PsGetCurrentProcess())
#define PgGetProcessGroupId(Process) ((Process)->Group ? (Process)->Group->GroupId : 0)
#define PgGetCurrentGroupId() PgGetProcessGroupId(PsGetCurrentProcess())
#define PgGetCurrentGroup() PsGetCurrentProcess()->Group

#endif