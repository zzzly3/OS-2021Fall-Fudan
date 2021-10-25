#include <mod/worker.h>
#include <def.h>

// extern PKPROCESS KernelProcess[CPU_NUM]; // use self instead
extern int WorkerSwitchTimer[CPU_NUM];
RT_ONLY void PsiCheckInactiveList();
RT_ONLY BOOL PsiTransferProcess();
extern int ActiveProcessCount[CPU_NUM];
extern int WaitingProcessCount[CPU_NUM];

void KeSystemWorkerEntry()
{
	PKPROCESS cur = PsGetCurrentProcess();
	while (1)
	{
		// Awake waiting process
		KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
		PsiCheckInactiveList();
		PsiTransferProcess();
		WorkerSwitchTimer[cpuid()] = WORKER_SWITCH_ROUND; // Reset the timer
		printf("Worker %d: active %d, waiting %d\n", cpuid(), ActiveProcessCount[cpuid()], WaitingProcessCount[cpuid()]);
		KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
		if (cur->SchedulerList.Backward == &cur->SchedulerList)
		{
			// Scheduler list is empty
			delay_us(TIME_SLICE_MS);
		}
		else
		{
			// Give up the time
			arch_disable_trap();
			KeTaskSwitch();
			arch_enable_trap();
		}
	}
}
