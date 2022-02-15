#include <def.h>

void msgtest()
{
	printf("ahhh\n");
}

void spawn_init_process();

PKPROCESS CleanerProcess;
void KeCleanerEntry()
{
	printf("Cleaner process created.");
	// clean zombie process
	PKPROCESS cur = PsGetCurrentProcess();
	KeUpdateQueueCounter(&cur->MessageQueue, 999);
	for (;;)
	{
		PMESSAGE msg = KeUserWaitMessage(&cur->MessageQueue);
		if (msg == NULL)
			break;
		if (msg->Type == MSG_TYPE_FREEPROC)
		{
			printf("\t free proc %p\n", msg->Data);
			PsDereferenceProcess((PKPROCESS)msg->Data);
		}
		KeFreeMessage(msg);
	}
}

void KeSystemEntry()
{
	puts("System process created.");
	delay_us(500 * 1000); // Wait all cores to complete initializing

	// // test
	// ULONG64 addr[] = {0, 12345, 222222222222, -999999};
	// for (int i = 0; i < 4; i++)
	// {
	// 	printf("%p %d\n", addr[i], MmProbeRead((PVOID)addr[i]));
	// }
	// arch_disable_trap();
	// PMEMORY_SPACE mem = MmCreateMemorySpace();
	// // MmMapPageEx(mem, 0, K2P(MmAllocatePhysicalPage()) | PTE_USER_DATA);
	// MmMapPageEx(mem, 0, VPTE_VALID);
	// KeSwitchMemorySpace(mem);
	// printf("a\n");
	// *(int*)0 = 123;
	// printf("%d\n", *(int*)0);

	driver_init();
	// sd_test();

	static OpContext ctx;
	bcache.begin_op(&ctx);
	Inode* ip = inodes.get(ROOT_INODE_NO);
	printf("root size=%d bno=%d\n", ip->entry.num_bytes, ip->entry.addrs[0]);
	// Block* b = bcache.acquire(ip->entry.addrs[0]);
	static u8 b[512];
	inodes.read(ip, b, 0, 512);
	for (int i = 0; i < 512; i += sizeof(DirEntry))
	{
		DirEntry* d = (DirEntry*)&b[i];
		if (d->inode_no == 0)
			break;
		printf("#%d: %d %s\n", i, d->inode_no, d->name);
	}
	inodes.put(&ctx, ip);
	bcache.end_op(&ctx);

	static KSTRING cleaner;
	static int pid;
	LibInitializeKString(&cleaner, "cleaner", 16);
	KeCreateProcess(&cleaner, KeCleanerEntry, NULL, &pid);
	ASSERT(KSUCCESS(PsReferenceProcessById(pid, &CleanerProcess)), BUG_STOP);

	putstr("spawn_init_process\n");
	spawn_init_process();
	putstr("System process exited.\n");
}

void create_system_process()
{
	static KSTRING system;
	static int pid;
	LibInitializeKString(&system, "system", 16);
	KeCreateProcess(&system, KeSystemEntry, NULL, &pid);
	// printf("system is %d\n", pid);
}