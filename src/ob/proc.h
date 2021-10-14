#ifndef __PROC_H__
#define __PROC_H__

#include <common/lutil.h>
#include <ob/mem.h>

#define PROCESS_STATUS_INITIALIZE 0
#define PROCESS_STATUS_RUNNING 1
#define PROCESS_STATUS_WAITING 2
#define PROCESS_STATUS_REALTIME 3
#define PROCESS_FLAG_KERNEL 1
typedef struct _KPROCESS
{
	USHORT Status;
	USHORT Flags;
	USHORT ReferenceCount;
	SPINLOCK Lock;
	int ProcessId;
	int ParentId;
	LIST_ENTRY ProcessList;
	PMEMORY_SPACE MemorySpace;
	struct
	{
		PVOID UserStack;
		union
		{
			PVOID p;
			struct
			{
				ULONG64 fp; // x29
				ULONG64 lr; // x30/equivalent to pc
				ULONG64 x27;
				ULONG64 x28;
				ULONG64 x25;
				ULONG64 x26;
				ULONG64 x23;
				ULONG64 x24;
				ULONG64 x21;
				ULONG64 x22;
				ULONG64 x19;
				ULONG64 x20;
			} *d; // When switched out, the kstack should holds the context on its top.
		} KernelStack;
	} Context;
	char DebugName[16]; // DON'T access this directly, invoke PsGetDebugNameEx() instead.
} KPROCESS, *PKPROCESS;


BOOL ObInitializeProcessManager();
PKPROCESS PsCreateProcessEx();

#endif