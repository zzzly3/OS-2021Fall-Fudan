#include <ob/mem.h>
#include <driver/uart.h>
//#include <core/virtual_memory.h>

extern PMemory pmem;
static SPINLOCK PhysicalPageListLock;
PHYSICAL_PAGE_INFO PhysicalPageInfoTable[PPN_MAX];
static ULONG64 AllocatedPagesCount;
static OBJECT_POOL MemorySpacePool;

static void MmInitializePages()
{
	init_memory_manager();
	KeInitializeSpinLock(&PhysicalPageListLock);
}

ULONG64 MmGetAllocatedPagesCount()
{
	return AllocatedPagesCount;
}

void HalInitializeMemoryManager()
{
	MmInitializePages();
	MmInitializeObjectPool(&MemorySpacePool, sizeof(MEMORY_SPACE));
	//init_virtual_memory(); // I don't want to fuck the gcc again!!!
}

// The physical pages are given in kernel address.
PVOID MmAllocatePhysicalPage()
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	PVOID p = kalloc();
	if (p)
	{
		PhysicalPageInfoTable[P2N(K2P(p))].ReferenceCount = 1;
		++AllocatedPagesCount;
	}
	KeReleaseSpinLock(&PhysicalPageListLock);
	if (p)
		memset(p, 0, PAGE_SIZE);
	return p;
}

BOOL MmReferencePhysicalPage(PVOID PageAddress)
{
	BOOL ret;
	int ppn = P2N(K2P(PageAddress));
	KeAcquireSpinLock(&PhysicalPageListLock);
	if ((ret = (PhysicalPageInfoTable[ppn].ReferenceCount < OBJECT_MAX_REFERENCE)))
		++PhysicalPageInfoTable[ppn].ReferenceCount;
	KeReleaseSpinLock(&PhysicalPageListLock);
	return ret;
}

void MmFreePhysicalPage(PVOID PageAddress)
{
	KeAcquireSpinLock(&PhysicalPageListLock);
	if (--PhysicalPageInfoTable[P2N(K2P(PageAddress))].ReferenceCount == 0)
	{
		kfree(PageAddress);
		--AllocatedPagesCount;
	}
	KeReleaseSpinLock(&PhysicalPageListLock);
}

PVOID MmiBuildObjectPool(USHORT sz)
{
	PVOID p = MmAllocatePhysicalPage();
	if (p == NULL)
		return NULL;
	for (ULONG64 p1 = (ULONG64)p, pe = p1 + PAGE_SIZE - sz;;)
	{
		ULONG64 p2 = p1 + sz;
		if (p2 > pe)
		{
			*(ULONG64*)p1 = NULL;
			break;
		}
		*(ULONG64*)p1 = p2;
		p1 = p2;
	}
	return p;
}

void MmInitializeObjectPool(POBJECT_POOL ObjectPool, USHORT Size)
{
	memset(ObjectPool, 0, sizeof(OBJECT_POOL));
	KeInitializeSpinLock(&ObjectPool->Lock);
	USHORT sz = (Size + 7) & ~7;
	ObjectPool->Size = sz;
	ObjectPool->Head = MmiBuildObjectPool(sz);
}

PVOID MmAllocateObject(POBJECT_POOL ObjectPool)
{
	ObLockObject(ObjectPool);
	if (ObjectPool->Head == NULL)
		ObjectPool->Head = MmiBuildObjectPool(ObjectPool->Size);
	PVOID p = ObjectPool->Head;
	if (p != NULL)
	{
		ObjectPool->Head = (PVOID)*(ULONG64*)p;
		memset(p, 0, ObjectPool->Size);
	}
	ObUnlockObject(ObjectPool);
	return p;
}

void MmFreeObject(POBJECT_POOL ObjectPool, PVOID Object)
{
	ObLockObject(ObjectPool);
	*(ULONG64*)Object = (ULONG64)ObjectPool->Head;
	ObjectPool->Head = Object;
	ObUnlockObject(ObjectPool);
}

BOOL MmiInitializeMemorySpace(PMEMORY_SPACE MemorySpace)
{
	PVOID table_base = MmAllocatePhysicalPage();
	if (table_base == NULL)
		return FALSE;
	MemorySpace->ActiveCount = 0;
	MemorySpace->ttbr0 = K2P(table_base);
	MemorySpace->PageTable = (PPAGE_TABLE)table_base;
	KeInitializeSpinLock(&MemorySpace->Lock);
	return TRUE;
}

BOOL MmiTestEmptyTable(PPAGE_TABLE PageTable)
{
	for (int i = 0; i < N_PTE_PER_TABLE; i++)
		if (VALID_PTE(PageTable[i]))
			return FALSE;
	return TRUE;
}

void MmiFreePage(ULONG64 PageDescriptor)
{
	if (PageDescriptor & PTE_VALID)
	{
		MmFreePhysicalPage((PVOID)P2K(PTE_ADDRESS(PageDescriptor)));
	}
}

void MmiFreeTable(PPAGE_TABLE PageTable, int Level)
{
	for (int i = 0; i < N_PTE_PER_TABLE; i++)
	{
		if (VALID_PTE(PageTable[i]))
		{
			if (Level < 3)
			{
				MmiFreeTable((PPAGE_TABLE)P2K(PTE_ADDRESS(PageTable[i])), Level + 1);
			}
			else
			{
				MmiFreePage(PageTable[i]);
			}
		}
	}
	MmFreePhysicalPage((PVOID)PageTable);
}

PPAGE_ENTRY MmiGetPageEntry(PPAGE_TABLE PageTable, PVOID VirtualAddress)
{
	PPAGE_TABLE pt = PageTable;
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 3; i++)
	{
		if (!VALID_PTE(pt[id[i]]))
		{
			return NULL;
		}
		pt = (PPAGE_TABLE)P2K(PTE_ADDRESS(pt[id[i]]));
	}
	return VALID_PTE(pt[id[3]]) ? (PPAGE_ENTRY)&pt[id[3]] : NULL;
}

PMEMORY_SPACE MmCreateMemorySpace()
{
	PMEMORY_SPACE p = (PMEMORY_SPACE)MmAllocateObject(&MemorySpacePool);
	if (p == NULL)
		return NULL;
	if (!MmiInitializeMemorySpace(p))
	{
		MmFreeObject(&MemorySpacePool, (PVOID)p);
		return NULL;
	}
	return p;
}

KSTATUS MmMapPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress, ULONG64 PageDescriptor)
{
	ObLockObject(MemorySpace);
	PPAGE_TABLE pt = MemorySpace->PageTable;
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 3; i++)
	{
		if (!VALID_PTE(pt[id[i]]))
		{
			PVOID p = MmAllocatePhysicalPage();
			if (p == NULL)
			{
				ObUnlockObject(MemorySpace);
				return STATUS_NO_ENOUGH_MEMORY;
			}
			pt[id[i]] = K2P(p) + PTE_TABLE;
		}
		pt = (PPAGE_TABLE)P2K(PTE_ADDRESS(pt[id[i]]));
	}
	if (VALID_PTE(pt[id[3]]))
	{
		ObUnlockObject(MemorySpace);
		return STATUS_ALREADY_MAPPED;
	}
	pt[id[3]] = PageDescriptor;
	// Flush the TLB if the change is done online.
	if (MemorySpace->ActiveCount > 0)
		MmFlushTLB();
	ObUnlockObject(MemorySpace);
	return STATUS_SUCCESS;
}

// Given in kernel address
PVOID MmGetPhysicalAddressEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ObLockObject(MemorySpace);
	PPAGE_ENTRY pe = MmiGetPageEntry(MemorySpace->PageTable, VirtualAddress);
	if (pe == NULL)
	{
		ObUnlockObject(MemorySpace);
		return NULL;
	}
	ULONG64 pb = P2K(PTE_ADDRESS(*pe));
	ObUnlockObject(MemorySpace);
	return (PVOID)(pb + VA_OFFSET(VirtualAddress));
}

KSTATUS MmUnmapPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ObLockObject(MemorySpace);
	PPAGE_TABLE pt[5] = {MemorySpace->PageTable};
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 4; i++)
	{
		if (!VALID_PTE(pt[i][id[i]]))
		{
			ObUnlockObject(MemorySpace);
			return STATUS_PAGE_NOT_FOUND;
		}
		pt[i + 1] = (PPAGE_TABLE)P2K(PTE_ADDRESS(pt[i][id[i]]));
	}
	// NEVER free a page before dereference it, even if it's impossible to cause any problem.
	ULONG64 pd = pt[3][id[3]];
	pt[3][id[3]] = NULL;
	if (MemorySpace->ActiveCount > 0)
		MmFlushTLB();
	MmiFreePage(pd);
	for (int i = 3; i > 0; i--)
	{
		if (MmiTestEmptyTable(pt[i]))
		{
			pt[i - 1][id[i - 1]] = NULL;
			if (MemorySpace->ActiveCount > 0)
				MmFlushTLB();
			MmFreePhysicalPage((PVOID)pt[i]);
		}
		else
			break;
	}
	ObUnlockObject(MemorySpace);
	return STATUS_SUCCESS;
}

void MmDestroyMemorySpace(PMEMORY_SPACE MemorySpace)
{
	ObLockObject(MemorySpace);
	MmiFreeTable(MemorySpace->PageTable, 0);
	// No need to unlock since destroyed.
	MmFreeObject(&MemorySpacePool, (PVOID)MemorySpace);
}

void MmSwitchMemorySpaceEx(PMEMORY_SPACE oldMemorySpace, PMEMORY_SPACE newMemorySpace)
{
	ObLockObject(newMemorySpace);
	++newMemorySpace->ActiveCount;
	arch_set_ttbr0(newMemorySpace->ttbr0);
	ObUnlockObject(newMemorySpace);
	if (oldMemorySpace != NULL)
	{
		ObLockObject(oldMemorySpace);
		--oldMemorySpace->ActiveCount;
		ObUnlockObject(oldMemorySpace);
	}
}
