#include <ob/proc.h>

void PsiInitializeProcess(PKPROCESS Process)
{
	memset(Process, 0, sizeof(KPROCESS));
	KeInitializeSpinLock(&Process->Lock);
	
}