#include <ob/mem.h>
#include <driver/uart.h>
#include <mod/bug.h>
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
UNSAFE PVOID MmAllocatePhysicalPage()
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	KeAcquireSpinLockFast(&PhysicalPageListLock);
	PVOID p = kalloc();
	if (p)
	{
		PhysicalPageInfoTable[P2N(K2P(p))].ShareCount = 1;
		++AllocatedPagesCount;
	}
	KeReleaseSpinLockFast(&PhysicalPageListLock);
	if (p)
		memset(p, 0, PAGE_SIZE);
	return p;
}

UNSAFE int MmUnsharePhysicalPage(PVOID PageAddress)
{
	KeAcquireSpinLockFast(&PhysicalPageListLock);
	int n = P2N(K2P(PageAddress));
	int r = 1;
	if (PhysicalPageInfoTable[n].ShareCount > 1)
		r = PhysicalPageInfoTable[n].ShareCount--;
	KeReleaseSpinLockFast(&PhysicalPageListLock);
	return r;
}

// UNSAFE BOOL MmReferencePhysicalPage(PVOID PageAddress)
// {
// 	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
// 	BOOL ret;
// 	int ppn = P2N(K2P(PageAddress));
// 	KeAcquireSpinLockFast(&PhysicalPageListLock);
// 	if ((ret = (PhysicalPageInfoTable[ppn].ReferenceCount < OBJECT_MAX_REFERENCE)))
// 		++PhysicalPageInfoTable[ppn].ReferenceCount;
// 	KeReleaseSpinLockFast(&PhysicalPageListLock);
// 	return ret;
// }

UNSAFE void MmFreePhysicalPage(PVOID PageAddress)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	KeAcquireSpinLockFast(&PhysicalPageListLock);
	if (--PhysicalPageInfoTable[P2N(K2P(PageAddress))].ShareCount == 0)
	{
		kfree(PageAddress);
		--AllocatedPagesCount;
	}
	KeReleaseSpinLockFast(&PhysicalPageListLock);
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

UNSAFE void MmInitializeObjectPool(POBJECT_POOL ObjectPool, USHORT Size)
{
	memset(ObjectPool, 0, sizeof(OBJECT_POOL));
	KeInitializeSpinLock(&ObjectPool->Lock);
	USHORT sz = (Size + 7) & ~7;
	ObjectPool->Size = sz;
	ObjectPool->Head = MmiBuildObjectPool(sz);
}

UNSAFE PVOID MmAllocateObject(POBJECT_POOL ObjectPool)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	ObLockObjectFast(ObjectPool);
	if (ObjectPool->Head == NULL)
		ObjectPool->Head = MmiBuildObjectPool(ObjectPool->Size);
	PVOID p = ObjectPool->Head;
	if (p != NULL)
	{
		ObjectPool->Head = (PVOID)*(ULONG64*)p;
		memset(p, 0, ObjectPool->Size);
		ObjectPool->AllocatedCount++;
	}
	ObUnlockObjectFast(ObjectPool);
	return p;
}

UNSAFE void MmFreeObject(POBJECT_POOL ObjectPool, PVOID Object)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	ObLockObjectFast(ObjectPool);
	*(ULONG64*)Object = (ULONG64)ObjectPool->Head;
	ObjectPool->Head = Object;
	ObjectPool->AllocatedCount--;
	ObUnlockObjectFast(ObjectPool);
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

UNSAFE PMEMORY_SPACE MmCreateMemorySpace()
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
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

UNSAFE KSTATUS MmMapPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress, ULONG64 PageDescriptor)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	ObLockObjectFast(MemorySpace);
	PPAGE_TABLE pt = MemorySpace->PageTable;
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 3; i++)
	{
		if (!VALID_PTE(pt[id[i]]))
		{
			PVOID p = MmAllocatePhysicalPage();
			if (p == NULL)
			{
				ObUnlockObjectFast(MemorySpace);
				return STATUS_NO_ENOUGH_MEMORY;
			}
			pt[id[i]] = K2P(p) + PTE_TABLE;
		}
		pt = (PPAGE_TABLE)P2K(PTE_ADDRESS(pt[id[i]]));
	}
	if (VALID_PTE(pt[id[3]]))
	{
		ObUnlockObjectFast(MemorySpace);
		return STATUS_ALREADY_MAPPED;
	}
	pt[id[3]] = PageDescriptor;
	// Flush the TLB if the change is done online.
	if (MemorySpace->ActiveCount > 0)
		MmFlushTLB();
	ObUnlockObjectFast(MemorySpace);
	return STATUS_SUCCESS;
}

UNSAFE KSTATUS MmCreateUserPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	PVOID p = MmAllocatePhysicalPage();
	if (p == NULL)
		return STATUS_NO_ENOUGH_MEMORY;
	KSTATUS r = MmMapPageEx(MemorySpace, VirtualAddress, K2P((ULONG64)p) | PTE_USER_DATA);
	if (!KSUCCESS(r))
		MmFreePhysicalPage(p);
	return r;
}

UNSAFE KSTATUS MmCreateUserPagesEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress, int Size, BOOL Allocate)
{
	ULONG64 end = (ULONG64)VirtualAddress + Size;
	for (ULONG64 a = PAGE_BASE((ULONG64)VirtualAddress); a <= PAGE_BASE(end) && a < end; a += PAGE_SIZE)
	{
		KSTATUS r;
		if (Allocate)
		{
			r = MmCreateUserPageEx(MemorySpace, (PVOID)a);
		}
		else
		{
			r = MmMapPageEx(MemorySpace, (PVOID)a, VPTE_VALID);
		}
		if (!KSUCCESS(r))
			return r;
	}
	return STATUS_SUCCESS;
}

// Given in kernel address
UNSAFE PVOID MmGetPhysicalAddressEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	ObLockObjectFast(MemorySpace);
	PPAGE_ENTRY pe = MmiGetPageEntry(MemorySpace->PageTable, VirtualAddress);
	if (pe == NULL)
	{
		ObUnlockObjectFast(MemorySpace);
		return NULL;
	}
	ULONG64 pb = P2K(PTE_ADDRESS(*pe));
	ObUnlockObjectFast(MemorySpace);
	return (PVOID)(pb + VA_OFFSET(VirtualAddress));
}

UNSAFE KSTATUS MmUnmapPageEx(PMEMORY_SPACE MemorySpace, PVOID VirtualAddress)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	ObLockObjectFast(MemorySpace);
	PPAGE_TABLE pt[5] = {MemorySpace->PageTable};
	int id[] = {VA_PART0(VirtualAddress), VA_PART1(VirtualAddress), VA_PART2(VirtualAddress), VA_PART3(VirtualAddress)};
	for (int i = 0; i < 4; i++)
	{
		if (!VALID_PTE(pt[i][id[i]]))
		{
			ObUnlockObjectFast(MemorySpace);
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
	ObUnlockObjectFast(MemorySpace);
	return STATUS_SUCCESS;
}

UNSAFE void MmDestroyMemorySpace(PMEMORY_SPACE MemorySpace)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	ASSERT(MemorySpace->ActiveCount == 0, BUG_BADREF);
	ObLockObjectFast(MemorySpace);
	MmiFreeTable(MemorySpace->PageTable, 0);
	// No need to unlock since destroyed.
	MmFreeObject(&MemorySpacePool, (PVOID)MemorySpace);
}

UNSAFE void MmSwitchMemorySpaceEx(PMEMORY_SPACE oldMemorySpace, PMEMORY_SPACE newMemorySpace)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	if (newMemorySpace != NULL)
	{
		ObLockObjectFast(newMemorySpace);
		++newMemorySpace->ActiveCount;
		arch_set_ttbr0(newMemorySpace->ttbr0);
		ObUnlockObjectFast(newMemorySpace);
	}	
	if (oldMemorySpace != NULL)
	{
		ObLockObjectFast(oldMemorySpace);
		--oldMemorySpace->ActiveCount;
		ObUnlockObjectFast(oldMemorySpace);
	}
}

PPAGE_TABLE MmiDuplicateTable(PPAGE_TABLE PageTable, int Level)
{
	PPAGE_TABLE pt = (PPAGE_TABLE)MmAllocatePhysicalPage();
	if (pt == NULL)
		return NULL;
	if (Level == 3)
		KeAcquireSpinLockFast(&PhysicalPageListLock);
	for (int i = 0; i < N_PTE_PER_TABLE; i++)
	{
		if (VALID_PTE(PageTable[i]))
		{
			if (Level < 3)
			{
				PPAGE_TABLE ptn = MmiDuplicateTable((PPAGE_TABLE)P2K(PTE_ADDRESS(PageTable[i])), Level + 1);
				if (ptn == NULL)
					goto fail;
				pt[i] = K2P(ptn) + PTE_TABLE;
			}
			else
			{
				if (PageTable[i] & PTE_VALID)
				{
					ULONG64 paddr = PTE_ADDRESS(PageTable[i]);
					PhysicalPageInfoTable[P2N(paddr)].ShareCount++;
					PageTable[i] |= PTE_RO;
				}
				pt[i] = PageTable[i];
			}
		}
	}
	if (Level == 3)
		KeReleaseSpinLockFast(&PhysicalPageListLock);
	return pt;
fail:
	MmiFreeTable(pt, Level);
	return NULL;
}

UNSAFE PMEMORY_SPACE MmDuplicateMemorySpace(PMEMORY_SPACE MemorySpace)
{
	ASSERT(!arch_disable_trap(), BUG_UNSAFETRAP);
	PMEMORY_SPACE p = (PMEMORY_SPACE)MmAllocateObject(&MemorySpacePool);
	if (p == NULL)
		return NULL;
	KeInitializeSpinLock(&p->Lock);
	ObLockObjectFast(MemorySpace);
	PPAGE_TABLE pt = MmiDuplicateTable(MemorySpace->PageTable, 0);
	ObUnlockObjectFast(MemorySpace);
	if (pt == NULL)
	{
		MmFreeObject(&MemorySpacePool, (PVOID)p);
		return NULL;
	}
	p->PageTable = pt;
	p->ttbr0 = K2P((ULONG64)pt);
	return p;
}

BOOL MmProbeReadEx(PVOID Address, int Size)
{
	for (ULONG64 i = (ULONG64)Address; i < (ULONG64)Address + Size; i += PAGE_SIZE)
	{
		if (!MmProbeRead((PVOID)i))
			return FALSE;
	}
	return MmProbeRead((PVOID)((ULONG64)Address + Size - 1));
}