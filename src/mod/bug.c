#include <mod/bug.h>
#include <def.h>

#define BLUE(text) "\033[47;34m"text"\033[0m"
#define RED(text) "\033[47;31m"text"\033[0m"

static const struct
{
	ULONG64 Id;
	CPCHAR Description;
} BugList[] = {
	{BUG_PANIC, "PANIC (legacy bug interface) called."},
	{BUG_STOP, "Kernel stopped initiatively."},
	{BUG_EXCEPTION, "Unknown kernel-mode exception detected."},
	{BUG_CHECKFAIL, "Self-check failed while testing."},
	{BUG_BADLEVEL, "Execute level mismatching."},
	{BUG_BADLOCK, "Kernel-mode lock objects corruptted."},
	{BUG_BADDPC, "DPC routine takes too much time."},
	{BUG_UNSAFETRAP, "Trap enabled during unsafe operations."},
	{BUG_SCHEDULER, "Invalid scheduler status."},
	{BUG_BADREF, "Invalid reference count."},
	{BUG_FSFAULT, "Filesystem internal fault."},
	{BUG_WORKER, "System worker process exception."},
	{BUG_BADIO, "Invalid IO request."},
	{BUG_BADPAGE, "Pagetable corruption."}
};
static const CPCHAR ELName[] = {"USR", "RES_1", "APC", "RES_3", "RT", "RES_5", "ISR"};
static const CPCHAR PSName[] = {"INVALID", "RUNNING", "RUNABLE", "ZOMBIE", "WAIT"};

volatile BOOL KeBugFaultFlag = FALSE;
extern int DpcWatchTimer[CPU_NUM];
extern int ActiveProcessCount[CPU_NUM];
extern PKPROCESS TransferProcess;
extern int WakenProcessCount;
// extern int WaitingProcessCount[CPU_NUM];
// extern PKPROCESS TransferWaitingProcess;

CPCHAR KeiGetBugDescription(ULONG64 BugId)
{
	for (int i = 0; i < sizeof(BugList) / 16; i++)
	{
		if (BugList[i].Id == BugId)
			return BugList[i].Description;
	}
	return "Unknown error.";
}

void KeBugFaultEx(CPCHAR BugFile, ULONG64 BugLine, ULONG64 BugId)
{
	BOOL trapen = arch_disable_trap();
	if (KeBugFaultFlag) while (1);
	KeBugFaultFlag = TRUE;
	delay_us(300 * 1000);
	puts("\n\n\033[41;33m================KERNEL FAULT================\033[0m\n");
	printf(BLUE("[*]")"Bug: \033[41;30m0x%p\033[0m "RED("%s")"\n", BugId, KeiGetBugDescription(BugId));
	printf(BLUE("[*]")"Kernel fault in "RED("FILE %s, LINE %d")"\n", BugFile, BugLine);
	u64 p, t, x;
    asm volatile("mov %[x], sp" : [x] "=r"(p));
    //asm volatile("mov %[x], x18" : [x] "=r"(x));
    asm volatile("mrs %[x], ttbr0_el1" : [x] "=r"(t));
    printf(BLUE("[*]")"sp = 0x%p, ttbr0 = 0x%p, elr = 0x%p, esr = 0x%p.\n", p, t, arch_get_elr(), arch_get_esr());
	PKPROCESS cur = PsGetCurrentProcess();
	printf(BLUE("[*]")"Current CPUID = %d, PID = %d, Status = %s, Execute Level = %s, Trap %s,\n", cpuid(), cur->ProcessId, PSName[cur->Status], ELName[cur->ExecuteLevel], trapen ? "enabled" : "disabled");
	printf("Name = %s, Flags = 0x%x, APC List: %s, Wait Mutex: %s, %s, Reference = %d.\n", &cur->DebugName, cur->Flags, cur->ApcList ? "not empty" : "empty", cur->WaitMutex ? "true" : "false", cur->Lock.locked ? "Locked" : "Not locked", cur->ReferenceCount.count);
	if (cur->Group == NULL)
		printf(BLUE("[*]")"Current Process Group: NULL\n");
	else
		printf(BLUE("[*]")"Current Process Group: %p, GID = %d, GPID = %d, Flags = 0x%x\n", cur->Group, cur->Group->GroupId, cur->GroupProcessId, cur->Group->Flags);
	printf(BLUE("[*]")"Process Active Count = %d %d %d %d, Waken Count = %d, Transferring = %p,\n", ActiveProcessCount[0], ActiveProcessCount[1], ActiveProcessCount[2], ActiveProcessCount[3], WakenProcessCount, (PVOID)TransferProcess);
	printf(BLUE("[*]")"Allocated Physical Pages = %d.\n", MmGetAllocatedPagesCount());
	printf(BLUE("[*]")"Stack:\n");
	for (int i = 0; i < 16; i++)
	{
		printf("+0x%x\t%p\n", i * 8, ((ULONG64*)p)[i]);
	}
	while (1);
}

void MmiProbeReadCatch();
PPAGE_ENTRY MmiGetPageEntry(PPAGE_TABLE PageTable, PVOID VirtualAddress);
UNSAFE int MmUnsharePhysicalPage(PVOID PageAddress);
BOOL KiMemoryFaultHandler(PTRAP_FRAME TrapFrame, ULONG64 esr)
{
	ULONG64 far = arch_get_far();
	PMEMORY_SPACE ms = PsGetCurrentProcess()->MemorySpace;
	if (ms == NULL)
		return FALSE;
	ObLockObjectFast(ms);
	PPAGE_ENTRY pe = MmiGetPageEntry(ms->PageTable, (PVOID)far);
	if (pe == NULL)
		goto fail;
	if (((esr >> 2) & 31) == 0b10011) // Write Op & Permission Fault
	{
		if (!((*pe) & PTE_RO))
			goto fail;
		// copy on write
		printf("COW: %p\n", far);
		PVOID p0 = (PVOID)P2K(PTE_ADDRESS(*pe));
		if (MmUnsharePhysicalPage(p0) == 1)
		{
			*pe = (*pe) & (ULONG64)~PTE_RO;
		}
		else
		{
			PVOID p1 = (PVOID)MmAllocatePhysicalPage();
			if (p1 == NULL)
				goto fail;
			memcpy(p1, p0, PAGE_SIZE);
			*pe = K2P(p1) | PTE_USER_DATA;
		}
		ObUnlockObjectFast(ms);
		return TRUE;
	}
	else if (((esr >> 2) & 15) == 0b0001) // Translation Fault
	{
		if (VALID_PTE(*pe) && ((*pe) & VPTE_VALID))
		{
			printf("AOA: %p\n", far);
			PVOID p = MmAllocatePhysicalPage();
			if (p == NULL)
				goto fail;
			*pe = K2P(p) | PTE_USER_DATA;
		}
		else
			goto fail;
		ObUnlockObjectFast(ms);
		return TRUE;
	}
fail:
	ObUnlockObjectFast(ms);
	if (TrapFrame->elr >= (ULONG64)MmProbeRead && TrapFrame->elr < (ULONG64)MmiProbeReadCatch)
	{
		// try-catch
		TrapFrame->elr = (ULONG64)MmiProbeReadCatch;
		return TRUE;
	}
	return FALSE;
}

void KiExceptionEntry(PTRAP_FRAME TrapFrame, ULONG64 esr)
{
	printf("Exception: %p\n", esr);
	if (((esr >> 26) & 31) == 0b00101) // EL0 & 1 Data Abort
	{
		if (KiMemoryFaultHandler(TrapFrame, esr))
			return;
	}
	if ((TrapFrame->elr & (1ull << 63)) ||
		(PsGetCurrentProcess()->Flags & PROCESS_FLAG_KERNEL))
	{
		// Kernel exception
		KeBugFault(BUG_PANIC);
	}
	else
	{
		// User exception
		KeExitProcess();
	}
}
