#include <mod/worker.h>
#include <def.h>

// extern PKPROCESS KernelProcess[CPU_NUM]; // use self instead
extern int WorkerSwitchTimer[CPU_NUM];
// RT_ONLY void PsiCheckInactiveList();
RT_ONLY void PsiTransferProcess();
extern int ActiveProcessCount[CPU_NUM];
// extern int WaitingProcessCount[CPU_NUM];
extern PKPROCESS TransferProcess;
extern PDPC_ENTRY DpcList;
// extern PKPROCESS TransferWaitingProcess;

void KeSystemWorkerEntry()
{
	PKPROCESS cur = PsGetCurrentProcess();
	while (1)
	{
		// printf("worker%d: %p %p\n", cpuid(), cur->ApcList, DpcList);
		// delay_us(500*1000);
		// printf("worker %d %d\n", cpuid(), ActiveProcessCount[cpuid()]);
		// Awake waiting process
		KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
		// PsiCheckInactiveList();
		PsiTransferProcess();
		WorkerSwitchTimer[cpuid()] = WORKER_SWITCH_ROUND; // Reset the timer
		KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
		for (;;)
		{
			PMESSAGE msg = KeGetMessage(&cur->MessageQueue);
			if (msg == NULL)
				break;
			if (msg->Type == MSG_TYPE_FREEPROC)
			{
				printf("\t free proc %p\n", msg->Data);
				PsDereferenceProcess((PKPROCESS)msg->Data);
			}
		}
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
