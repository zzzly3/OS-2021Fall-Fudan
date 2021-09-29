#include <ob/mem.h>

extern PMemory pmem;
SPINLOCK PhysicalPageListLock;

void MmInitializePages()
{
	init_memory_manager();
	KeInitializeSpinLock(&PhysicalPageListLock);
}

KSTATUS MmAllocatePhysicalPage(PVOID* PageAddress)
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	PVOID p = kalloc();
	KeReleaseSpinLock(&PhysicalPageListLock);
	if (p == NULL)
		return STATUS_NO_ENOUGH_MEMORY;
	memset(p, 0, PAGE_SIZE);
	*PageAddress = p;
	return STATUS_SUCCESS;
}

void MmFreePhysicalPage(PVOID PageAddress)
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	kfree(PageAddress);
	KeReleaseSpinLock(&PhysicalPageListLock);
}


