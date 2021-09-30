#ifndef __MEM_H__
#define __MEM_H__

#include <common/lutil.h>
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/physical_memory.h>
#include <common/spinlock.h>

#define PPAGE_TABLE PTEntriesPtr
typedef struct _MEMORY_SPACE
{
	SPINLOCK Lock;
	PVOID ttbr0;
	PPAGE_TABLE PageTable;
} MEMORY_SPACE, *PMEMORY_SPACE;

#define VPTE_USER PTE_USER
#define VPTE_KERNEL PTE_KERNEL
// Custom flags range from bit[12] to bit[15] + bit[1]
#define VPTE_VALID 1 // Different from PTE_VALID
#define VPTE_XN (1 << 12)
#define VPTE_RES1 (1 << 13)
#define VPTE_RES2 (1 << 14)
#define VPTE_RES3 (1 << 15)
typedef struct _VIRTUAL_PAGE
{
	ULONG Flags;
	ULONG Id;
} VIRTUAL_PAGE, *PVIRTUAL_PAGE;

typedef struct _MEMORY_DESCRIPTOR
{

} MEMORY_DESCRIPTOR, *PMEMORY_DESCRIPTOR;

void HalInitializeMemoryManager();
BOOL MmInitializeMemorySpace(PMEMORY_SPACE);

#endif