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

void HalInitializeMemoryManager();
BOOL MmInitializeMemorySpace(PMEMORY_SPACE);

#endif