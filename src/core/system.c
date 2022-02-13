#include <def.h>

void msgtest()
{
	printf("ahhh\n");
}

void KeSystemEntry()
{
	puts("System process created.");
	delay_us(500 * 1000); // Wait all cores to complete initializing

	// test
	arch_disable_trap();
	PMEMORY_SPACE mem = MmCreateMemorySpace();
	MmMapPageEx(mem, 0, K2P(MmAllocatePhysicalPage()) | PTE_USER_DATA);
	// MmMapPageEx(mem, 0, 0);
	KeSwitchMemorySpace(mem);
	printf("a\n");
	*(int*)0 = 123;
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