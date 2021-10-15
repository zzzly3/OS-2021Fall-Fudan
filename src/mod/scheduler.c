#include <mod/scheduler.h>
#include <driver/uart.h>

extern PKPROCESS KernelProcess;
// Although one core has only one active list, locks are needed to apply the cross-core stealing policy (TODO).
static SPINLOCK ActiveListLock;

void swtch (PVOID kstack, PVOID* oldkstack);
void KiClockTrapEntry();

void ObInitializeScheduler()
{
	KeInitializeSpinLock(&ActiveListLock);
	arch_set_tid((ULONG64)KernelProcess);
	KernelProcess->Status = PROCESS_STATUS_RUNNING;
	uart_put_char('o');
	init_clock();
	set_clock_handler(KiClockTrapEntry);
	init_trap();
}

void PsiStartNewProcess(PKPROCESS Process)
{
	KeAcquireSpinLock(&ActiveListLock);
	LibInsertListEntry(&PsGetCurrentProcess()->SchedulerList, &Process->SchedulerList);
	Process->Status = PROCESS_STATUS_WAITING;
	// After that, the process is visible in the process list, and MIGHT BE REFERENCED BY OTHERS.
	KeReleaseSpinLock(&ActiveListLock);
}

void KiClockTrapEntry()
{
	reset_clock(200);
	//uart_put_char('t');
	PKPROCESS cur = PsGetCurrentProcess();
	if (cur->ExecuteLevel >= EXECUTE_LEVEL_RT)
		return;
	// Before schedulering, raise to RT level and enable interrupt
	EXECUTE_LEVEL oldel = KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
	arch_enable_trap();
	// TODO: call DPCs
	KeTaskSwitch();
	if (oldel < EXECUTE_LEVEL_APC)
	{
		cur->ExecuteLevel = EXECUTE_LEVEL_APC;
		// TODO: call APCs
	}
	arch_disable_trap();
	cur->ExecuteLevel = oldel;
	return;
}

// No need to lock the process, since only the process itself can change its realtime-state.
EXECUTE_LEVEL KeRaiseExecuteLevel(EXECUTE_LEVEL TargetExecuteLevel)
{
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	EXECUTE_LEVEL old = proc->ExecuteLevel;
	proc->ExecuteLevel = TargetExecuteLevel;
	// ObUnlockObject(proc);
	return old;
}

// The OldState must be the one returned by raise()!
// Otherwise, the kstack and trapframe would be corruptted! 
void KeLowerExecuteLevel(EXECUTE_LEVEL OriginalExecuteLevel)
{
	PKPROCESS proc = PsGetCurrentProcess();
	// ObLockObject(proc);
	// TODO: When lowerring through APC level, execute APCs
	proc->ExecuteLevel = OriginalExecuteLevel;
	// ObUnlockObject(proc);
}

void KeTaskSwitch() // MUST be called in RT level
{
	// Find the next
	// No need to lock the process, since only the scheduler can change its status.
	PKPROCESS cur = PsGetCurrentProcess();
	KeAcquireSpinLock(&ActiveListLock);
	PKPROCESS nxt = container_of(cur->SchedulerList.Backward, KPROCESS, SchedulerList);
	if (nxt == cur) // No more process
	{
		KeReleaseSpinLock(&ActiveListLock);
		return;
	}
	// Do switch
	arch_disable_trap();
	nxt->Status = PROCESS_STATUS_RUNNING;
	KeReleaseSpinLock(&ActiveListLock);
	cur->Context.UserStack = (PVOID)arch_get_usp();
	arch_set_usp((ULONG64)nxt->Context.UserStack);
	MmSwitchMemorySpaceEx(cur->MemorySpace, nxt->MemorySpace);
	arch_set_tid((ULONG64)nxt);
	cur->Status = PROCESS_STATUS_WAITING;
	uart_put_char('x');
	uart_put_char('\n');
	swtch(nxt->Context.KernelStack.p, &cur->Context.KernelStack.p);
	arch_enable_trap();
}