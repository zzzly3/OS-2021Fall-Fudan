#include <def.h>

void KeSystemEntry()
{
	puts("System process created.");
	delay_us(500 * 1000); // Wait all cores to complete initializing

	// test
	arch_disable_trap();
	PMEMORY_SPACE mem = MmCreateMemorySpace();
	MmCreateUserPageEx(mem, 0);
	MmSwitchMemorySpaceEx(NULL, mem);
	*(int*)0 = 123;
	printf("%d\n", *(int*)0);
	PMEMORY_SPACE mem2 = MmDuplicateMemorySpace(mem);
	MmSwitchMemorySpaceEx(mem, mem2);
	printf("%d\n", *(int*)0);
	*(int*)0 = 233;
	printf("%d\n", *(int*)0);

	// driver_init();
	// sd_test();
}

void create_system_process()
{
	static KSTRING system;
	static int pid;
	LibInitializeKString(&system, "system", 16);
	KeCreateProcess(&system, KeSystemEntry, NULL, &pid);
	// printf("system is %d\n", pid);
}