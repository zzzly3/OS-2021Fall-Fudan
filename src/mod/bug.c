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
	{BUG_CHECKFAIL, "Kernel self-check failed."}
};
static const CPCHAR ELName[] = {"USR", "APC", "RT", "ISR"};
static const CPCHAR PSName[] = {"INVALID", "RUNNING", "RUNABLE", "ZOMBIE", "WAIT"};

BOOL KeBugFaultFlag = FALSE;

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
	delay_us(200 * 1000);
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
	printf("Process Name = %s, Flags = 0x%x, APC List: %s, Wait Mutex: %s, %s.\n", &cur->DebugName, cur->Flags, cur->ApcList ? "not empty" : "empty", cur->WaitMutex ? "true" : "false", cur->Lock.locked ? "Locked" : "Not locked");
	printf(BLUE("[*]")"Allocated Physical Pages = %d.\n", MmGetAllocatedPagesCount());
	while (1);
}

void KiExceptionEntry(PTRAP_FRAME TrapFrame)
{
	if ((TrapFrame->elr & (1ull << 63)) ||
		(PsGetCurrentProcess()->Flags & PROCESS_FLAG_KERNEL))
	{
		// Kernel exception

	}
	else
	{
		// User exception
		KeExitProcess();
	}
}
