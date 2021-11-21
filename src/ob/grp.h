#ifndef __GRP_H__
#define __GRP_H__

#include <ob/proc.h>
#include <common/rc.h>

#define GROUP_FLAG_ZOMBIE 1

typedef struct _PROCESS_GROUP
{
	int GroupId;
	int Flags;
	int NextProcessId;
	REF_COUNT ReferenceCount; // NOTE: References are read-only
	struct _KPROCESS* GroupWorker; // WARNING: NEVER block the worker!!!
	LIST_ENTRY GroupList;
	LIST_ENTRY SchedulerList;
} PROCESS_GROUP, *PPROCESS_GROUP;

void ObInitializeGroupManager();
PPROCESS_GROUP PgCreateGroup();

#endif