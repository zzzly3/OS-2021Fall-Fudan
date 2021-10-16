#ifndef __PROC_H__
#define __PROC_H__

#include <common/lutil.h>
#include <ob/mem.h>
#include <mod/scheduler.h>

#define PROCESS_STATUS_INITIALIZE 0 // This status is inaccessible and only used when creating
#define PROCESS_STATUS_RUNNING 1
#define PROCESS_STATUS_WAITING 2
#define PROCESS_STATUS_ZOMBIE 3
#define PROCESS_FLAG_KERNEL 1
#define PROCESS_FLAG_REALTIME 2

typedef struct _KPROCESS
{
	EXECUTE_LEVEL ExecuteLevel;
	USHORT Status;
	USHORT Flags;
	SPINLOCK Lock;
	USHORT ReferenceCount;
	int ProcessId;
	int ParentId;
	LIST_ENTRY ProcessList;
	LIST_ENTRY SchedulerList;
	PMEMORY_SPACE MemorySpace;
	struct _APC_ENTRY* ApcList;
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
	ULONG64 x0;
	ULONG64 x1;
} TRAP_FRAME, *PTRAP_FRAME;

BOOL ObInitializeProcessManager();
PKPROCESS PsCreateProcessEx();
RT_ONLY void PsCreateProcess(PKPROCESS, PVOID, ULONG64);
KSTATUS KeCreateProcess(PKSTRING, PVOID, ULONG64, int*);
#define PsGetCurrentProcess() ((PKPROCESS)arch_get_tid()) 
void KeExitProcess();

#endif