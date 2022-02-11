#ifndef __MEM_H__
#define __MEM_H__

#include <common/lutil.h>
#include <aarch64/intrinsic.h>
#include <aarch64/mmu.h>
#include <common/string.h>
#include <common/spinlock.h>
#include <core/physical_memory.h>

typedef struct _OBJECT_POOL
{
	SPINLOCK Lock;
	USHORT Size;
	int AllocatedCount;
	PVOID Head;
} OBJECT_POOL, *POBJECT_POOL;

#define VA_PART0(va) (((ULONG64)(va) & 0xFF8000000000) >> 39)
#define VA_PART1(va) (((ULONG64)(va) & 0x7FC0000000) >> 30)
#define VA_PART2(va) (((ULONG64)(va) & 0x3FE00000) >> 21)
#define VA_PART3(va) (((ULONG64)(va) & 0x1FF000) >> 12)

extern PTEntries kernel_pt;
#define PPAGE_TABLE PTEntriesPtr
#define PPAGE_ENTRY PTEntriesPtr
typedef struct _MEMORY_SPACE
{
	USHORT ActiveCount;
	SPINLOCK Lock;
	ULONG64 ttbr0;
	PPAGE_TABLE PageTable;
} MEMORY_SPACE, *PMEMORY_SPACE;

#define VALID_PTE(pte) (pte & (PTE_VALID | VPTE_VALID))
// // Custom flags begin from bit[12] + bit[1]
#define VPTE_VALID 1 // Different from PTE_VALID
// #define VPTE_XN (1 << 12) // PTE_HIGH_XN
// #define VPTE_AOU (1 << 13) // allocate-on-use
// typedef struct _VIRTUAL_PAGE
// {
// 	ULONG Flags;
// 	ULONG Id;
// } VIRTUAL_PAGE, *PVIRTUAL_PAGE;

typedef struct _PHYSICAL_PAGE_INFO
{
	USHORT ShareCount;
} PHYSICAL_PAGE_INFO, *PPHYSICAL_PAGE_INFO;

#define MmFlushTLB arch_tlbi_vmalle1is
ULONG64 MmGetAllocatedPagesCount();
UNSAFE PVOID MmAllocatePhysicalPage();
UNSAFE BOOL MmReferencePhysicalPage(PVOID);
UNSAFE void MmFreePhysicalPage(PVOID);
UNSAFE void MmInitializeObjectPool(POBJECT_POOL, USHORT);
UNSAFE PVOID MmAllocateObject(POBJECT_POOL);
UNSAFE void MmFreeObject(POBJECT_POOL, PVOID);
void HalInitializeMemoryManager();
UNSAFE PMEMORY_SPACE MmCreateMemorySpace();
UNSAFE KSTATUS MmMapPageEx(PMEMORY_SPACE, PVOID, ULONG64);
UNSAFE PVOID MmGetPhysicalAddressEx(PMEMORY_SPACE, PVOID);
UNSAFE KSTATUS MmUnmapPageEx(PMEMORY_SPACE, PVOID);
UNSAFE void MmDestroyMemorySpace(PMEMORY_SPACE);
UNSAFE void MmSwitchMemorySpaceEx(PMEMORY_SPACE, PMEMORY_SPACE);
UNSAFE KSTATUS MmCreateUserPageEx(PMEMORY_SPACE, PVOID);
UNSAFE PMEMORY_SPACE MmDuplicateMemorySpace(PMEMORY_SPACE);

#endif