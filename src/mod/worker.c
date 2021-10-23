#include <mod/worker.h>
#include <def.h>

// extern PKPROCESS KernelProcess[CPU_NUM]; // use self instead
RT_ONLY void PsiCheckInactiveList();

void KeSystemWorkerEntry()
{
	PKPROCESS cur = PsGetCurrentProcess();
	while (1)
	{
		// Awake waiting process
		KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
		PsiCheckInactiveList();
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
