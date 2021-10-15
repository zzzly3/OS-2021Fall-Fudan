#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <common/lutil.h>
#include <common/defines.h>
#include <driver/clock.h>
#include <core/trap.h>
#include <ob/proc.h>
#include <ob/mem.h>

#define TIME_SLICE_MS 100

typedef void (*PDPC_ROUTINE)(ULONG64);
typedef void (*PAPC_ROUTINE)(ULONG64);
typedef struct _DPC_ENTRY
{
	PDPC_ROUTINE DpcRoutine;
	ULONG64 DpcArgument;
	struct _DPC_ENTRY* NextEntry;
} DPC_ENTRY, *PDPC_ENTRY;
typedef struct _APC_ENTRY
{
	PAPC_ROUTINE ApcRoutine;
	ULONG64 ApcArgument;
	struct _APC_ENTRY* NextEntry;
} APC_ENTRY, *PAPC_ENTRY;

void ObInitializeScheduler();
EXECUTE_LEVEL KeRaiseExecuteLevel(EXECUTE_LEVEL);
void KeLowerExecuteLevel(EXECUTE_LEVEL);
RT_ONLY void KeTaskSwitch();
APC_ONLY void KeClearApcList();
RT_ONLY void KeClearDpcList();
PDPC_ENTRY KeCreateDpc(PDPC_ROUTINE, ULONG64);
PAPC_ENTRY KeCreateApcEx(PKPROCESS, PAPC_ROUTINE, ULONG64);

#endif