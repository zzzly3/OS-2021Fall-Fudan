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
	uart_put_char('>');
	KeAcquireSpinLock(&PhysicalPageListLock);
	PVOID p = kalloc();
	KeReleaseSpinLock(&PhysicalPageListLock);
	uart_put_char('*');
	if (p) // p CANNOT be accessed without convert
		memset((void*)P2K(p), 0, PAGE_SIZE);
	uart_put_char('&');
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






