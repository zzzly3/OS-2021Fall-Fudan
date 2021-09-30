#include <ob/mem.h>
#include <driver/uart.h>

extern PMemory pmem;
SPINLOCK PhysicalPageListLock;

static void MmInitializePages()
{
	init_memory_manager();
	KeInitializeSpinLock(&PhysicalPageListLock);
}

void HalInitializeMemoryManager()
{
	MmInitializePages();
}

PVOID MmAllocatePhysicalPage()
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	PVOID p = kalloc();
	KeReleaseSpinLock(&PhysicalPageListLock);
	memset(p, 0, PAGE_SIZE);
	return p;
}

void MmFreePhysicalPage(PVOID PageAddress)
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	kfree(PageAddress);
	KeReleaseSpinLock(&PhysicalPageListLock);
}

BOOL MmInitializeMemorySpace(PMEMORY_SPACE MemorySpace)
{
	PVOID table_base = MmAllocatePhysicalPage();
	if (table_base == NULL)
		return FALSE;
	MemorySpace->ttbr0 = (PVOID)K2P(table_base);
	MemorySpace->PageTable = (PPAGE_TABLE)table_base;
	KeInitializeSpinLock(&MemorySpace->Lock);
	return TRUE;
}






