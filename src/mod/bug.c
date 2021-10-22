#include <mod/bug.h>
#include <def.h>

void KeBugFaultEx(CPCHAR BugFile, ULONG64 BugLine, ULONG64 BugId)
{
	arch_disable_trap();
	delay_us(1000);
	puts("\033[41;30m======KERNEL FAULT======\033[0m\n");
	printf("Bug 0x%p detected in FILE %s, LINE %d\n", BugId, BugFile, BugLine);
	while (1);
}