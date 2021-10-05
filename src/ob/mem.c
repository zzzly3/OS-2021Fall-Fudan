#include <ob/mem.h>
#include <driver/uart.h>

extern PMemory pmem;
static SPINLOCK PhysicalPageListLock;
PHYSICAL_PAGE_INFO PhysicalPageInfoTable[PPN_MAX];
static ULONG64 AllocatedPagesCount;

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
	//init_virtual_memory();
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

BOOL MmInitializeMemorySpace(PMEMORY_SPACE MemorySpace)
{
	PVOID table_base = MmAllocatePhysicalPage();
	if (table_base == NULL)
		return FALSE;
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

BOOL MmMapPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress, ULONG64 PageDescriptor)
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
				return FALSE;
			}
			pt[id[i]] = K2P(p) + PTE_TABLE;
		}
		pt = (PPAGE_TABLE)P2K(PTE_ADDRESS(pt[id[i]]));
	}
	pt[id[3]] = PageDescriptor;
	// Flush the TLB if the change is done online.
	if (MemorySpace->ActiveCount > 0)
		MmFlushTLB();
	ObUnlockObject(MemorySpace);
	return TRUE;
}

PVOID MmGetPhysicalAddressEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ObLockObject(MemorySpace);
	PPAGE_TABLE pt = MemorySpace->PageTable;
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 4; i++)
	{
		if (!VALID_PTE(pt[id[i]]))
		{
			ObUnlockObject(MemorySpace);
			return NULL;
		}
		pt = (PPAGE_TABLE)P2K(PTE_ADDRESS(pt[id[i]]));
	}
	ObUnlockObject(MemorySpace);
	return (PVOID)((ULONG64)pt + VA_OFFSET(VirtualAddress));
}

BOOL MmUnmapPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ObLockObject(MemorySpace);
	PPAGE_TABLE pt[5] = {MemorySpace->PageTable};
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 4; i++)
	{
		if (!VALID_PTE(pt[i][id[i]]))
		{
			ObUnlockObject(MemorySpace);
			return FALSE;
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
	return TRUE;
}

void MmDestroyMemorySpace(PMEMORY_SPACE MemorySpace)
{
	ObLockObject(MemorySpace);
	MmiFreeTable(MemorySpace->PageTable, 0);
	// No need to unlock since destroyed.
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
