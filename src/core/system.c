#include <def.h>

void KeSystemEntry()
{
	puts("System process created.");
	//sd_init();
}

void create_system_process()
{
	static KSTRING system;
	static int pid;
	LibInitializeKString(&system, "system", 16);
	KeCreateProcess(&system, KeSystemEntry, NULL, &pid);
	printf("system is %d\n", pid);
}