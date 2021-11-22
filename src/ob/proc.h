#ifndef __PROC_H__
#define __PROC_H__

#include <common/lutil.h>
#include <common/rc.h>
#include <ob/mem.h>
#include <ob/mutex.h>
#include <ob/grp.h>
#include <mod/scheduler.h>
#include <mod/bug.h>

#define PROCESS_STATUS_INITIALIZE 0 // This status is inaccessible and only used when creating
#define PROCESS_STATUS_RUNNING 1
#define PROCESS_STATUS_RUNABLE 2
#define PROCESS_STATUS_ZOMBIE 3
#define PROCESS_STATUS_WAIT 4
#define PROCESS_STATUS_MOVE 5 // Reserved for cross-group move
#define PROCESS_FLAG_KERNEL 1
#define PROCESS_FLAG_REALTIME 2 // Reserved for real-time process
#define PROCESS_FLAG_APCSTATE 4
#define PROCESS_FLAG_WAITING 8
#define PROCESS_FLAG_GROUPWORKER 16

typedef struct _KPROCESS
{
	PRIVATE EXECUTE_LEVEL ExecuteLevel;
	PRIVATE USHORT Status;
	PRIVATE USHORT Flags;
	SPINLOCK Lock;
	REF_COUNT ReferenceCount;
	int ProcessId;
	int ParentId; // Means group parent id
	int GroupProcessId;
	LIST_ENTRY ProcessList;
	LIST_ENTRY SchedulerList;
	LIST_ENTRY WaitList;
	LIST_ENTRY GroupProcessList;
	struct _PROCESS_GROUP* Group;
	PMEMORY_SPACE MemorySpace;
	PUBLIC struct _APC_ENTRY* ApcList;
	PUBLIC struct _MUTEX* WaitMutex;
	struct
	{
		PVOID UserStack;
		union
		{
			PVOID p;
			struct
			{
				ULONG64 x0; // arg1
				ULONG64 x1; // arg2
				ULONG64 fp; // x29
				ULONG64 lr; // x30/equivalent to pc
			} *d;
			// Unused members in context & trapframe emitted.
			// WARNING: Don't use 'd' or 'f' unless you are clear about what you are doing.
		} KernelStack;
	} Context;
	char DebugName[16]; // DON'T access this directly, invoke PsGetDebugNameEx() instead.
} KPROCESS, *PKPROCESS;

typedef struct _TRAP_FRAME
{
	ULONG64 elr;
	ULONG64 spsr;
	ULONG64 x8;
	ULONG64 x9;
	ULONG64 x0;
	ULONG64 x1;
} TRAP_FRAME, *PTRAP_FRAME;

BOOL ObInitializeProcessManager();
PKPROCESS PsCreateProcessEx();
RT_ONLY void PsCreateProcess(PKPROCESS, PVOID, ULONG64);
KSTATUS KeCreateProcess(PKSTRING, PVOID, ULONG64, int*);
#define PsGetCurrentProcess() ((PKPROCESS)arch_get_tid()) 
void KeExitProcess();
UNSAFE void PsFreeProcess(PKPROCESS);
KSTATUS PsReferenceProcessById(int, PKPROCESS*);
PKPROCESS PgGetProcessGroupWorker(PKPROCESS);
#define PgGetCurrentGroupWorker() PgGetProcessGroupWorker(PsGetCurrentProcess())

#endif