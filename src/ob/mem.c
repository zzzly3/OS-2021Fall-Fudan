#include <ob/mem.h>

extern PMemory pmem;
SPINLOCK PhysicalPageListLock;

static void MmInitializePages()
{
	init_memory_manager();
	KeInitializeSpinLock(&PhysicalPageListLock);
}

PVOID MmAllocatePhysicalPage()
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	PVOID p = kalloc();
	KeReleaseSpinLock(&PhysicalPageListLock);
	if (p) // p CANNOT be accessed without convert
		memset((void*)P2K(p), 0, PAGE_SIZE);
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
	PVOID ttb = MmAllocatePhysicalPage();
	if (ttb == NULL)
		return FALSE;
	MemorySpace->ttbr0 = ttb;
	MemorySpace->PageTable = (PPAGE_TABLE)P2K(ttb);
	KeInitializeSpinLock(&MemorySpace->Lock);
	return TRUE;
}

//KSTATUS MmMapIntoMemorySpace(PMEMORY_SPACE MemorySpace, )

void HalInitializeMemoryManager()
{
	MmInitializePages();
}


