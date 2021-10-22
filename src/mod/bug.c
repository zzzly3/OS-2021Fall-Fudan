#include <mod/bug.h>
#include <def.h>

void KeBugFaultEx(CPCHAR BugFile, ULONG64 BugLine, ULONG64 BugId)
{
	arch_disable_trap();
	delay_us(1000);
	puts("\033[41;33m======KERNEL FAULT======\033[0m");
	printf("Bug \033[41;33m0x%p\033[0m detected in FILE %s, LINE %d\n", BugId, BugFile, BugLine);
	while (1);
}