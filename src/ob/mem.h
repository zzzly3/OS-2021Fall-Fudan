#ifndef __MEM_H__
#define __MEM_H__

#include <common/lutil.h>
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/physical_memory.h>
#include <core/virtual_memory.h>
#include <common/spinlock.h>

#define VA_PART0(va) ((va & 0xFF8000000000) >> 39)
#define VA_PART1(va) ((va & 0x7FC0000000) >> 30)
#define VA_PART2(va) ((va & 0x3FE00000) >> 21)
#define VA_PART3(va) ((va & 0x1FF000) >> 12)

#define PPAGE_TABLE PTEntriesPtr
typedef struct _MEMORY_SPACE
{
	USHORT ActiveCount;
	SPINLOCK Lock;
	ULONG64 ttbr0;
	PPAGE_TABLE PageTable;
} MEMORY_SPACE, *PMEMORY_SPACE;

#define VALID_PTE(pte) (pte & PTE_VALID)
// Custom flags begin from bit[12] + bit[1]
#define VPTE_VALID 1 // Different from PTE_VALID
#define VPTE_XN (1 << 12) // PTE_HIGH_XN
#define VPTE_AOU (1 << 13) // allocate-on-use
typedef struct _VIRTUAL_PAGE
{
	ULONG Flags;
	ULONG Id;
} VIRTUAL_PAGE, *PVIRTUAL_PAGE;

typedef struct _PHYSICAL_PAGE_INFO
{
	USHORT ReferenceCount;
} PHYSICAL_PAGE_INFO, *PPHYSICAL_PAGE_INFO;

#define MmFlushTLB arch_tlbi_vmalle1is
void HalInitializeMemoryManager();
BOOL MmInitializeMemorySpace(PMEMORY_SPACE);
BOOL MmMapPageEx(PMEMORY_SPACE, PVOID, ULONG64);
PVOID MmGetPhysicalAddressEx(PMEMORY_SPACE, PVOID);
BOOL MmUnmapPageEx(PMEMORY_SPACE, PVOID);
void MmDestroyMemorySpace(PMEMORY_SPACE);
void MmSwitchMemorySpaceEx(PMEMORY_SPACE, PMEMORY_SPACE);

#endif