#include <mod/syscall.h>

void KiSystemCallEntry(PTRAP_FRAME TrapFrame)
{
	arch_enable_trap();
	ULONG64 callno = TrapFrame->x8;
	ULONG64 arg1 = TrapFrame->x0;
	ULONG64 arg2 = TrapFrame->x1;
	switch (callno)
	{
		case SYS_myexecve: sys_myexecve((char*)arg1); break; // UNSAFE!!!
		case SYS_myexit: sys_myexit(); break;
	}
	// Fault if EL mismatching
	KeRaiseExecuteLevel(EXECUTE_LEVEL_APC);
	KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
}