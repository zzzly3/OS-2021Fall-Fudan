#include <def.h>

void KeSystemEntry()
{
	puts("System process created.");
	delay_us(500 * 1000); // Wait all cores to complete initializing
	driver_init();
	sd_test();
}

void create_system_process()
{
	static KSTRING system;
	static int pid;
	LibInitializeKString(&system, "system", 16);
	KeCreateProcess(&system, KeSystemEntry, NULL, &pid);
	// printf("system is %d\n", pid);
}