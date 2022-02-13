#include <def.h>

void msgtest()
{
	printf("ahhh\n");
}

void KeSystemEntry()
{
	puts("System process created.");
	delay_us(500 * 1000); // Wait all cores to complete initializing

	// // test
	ULONG64 addr[] = {0, 12345, 222222222222, -999999};
	for (int i = 0; i < 4; i++)
	{
		printf("%p %d\n", addr[i], MmProbeRead((PVOID)addr[i]));
	}
	// arch_disable_trap();
	// PMEMORY_SPACE mem = MmCreateMemorySpace();
	// // MmMapPageEx(mem, 0, K2P(MmAllocatePhysicalPage()) | PTE_USER_DATA);
	// MmMapPageEx(mem, 0, VPTE_VALID);
	// KeSwitchMemorySpace(mem);
	// printf("a\n");
	// *(int*)0 = 123;
	// printf("%d\n", *(int*)0);

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