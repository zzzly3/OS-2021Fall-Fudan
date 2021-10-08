#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <core/virtual_memory.h>
#include <core/physical_memory.h>
#include <common/types.h>
#include <core/console.h>
#include <ob/mem.h>
#include <driver/uart.h>
#define pdgir pgdir
/*
    This module is reserved to fool the examination.
    It's STRONGLY NOT RECOMMENDED to use functions in this module.
    Invoke the memory manager instead.
*/
/* For simplicity, we only support 4k pages in user pgdir. */

PVOID MmAllocatePhysicalPage();
PPAGE_ENTRY MmiGetPageEntry(PPAGE_TABLE, PVOID);
void MmiFreeTable(PPAGE_TABLE, int);
extern PTEntries kpgdir;
VMemory vmem;

PTEntriesPtr pgdir_init() {
    return vmem.pgdir_init();
}

PTEntriesPtr pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {
    
    return vmem.pgdir_walk(pgdir, vak, alloc);
}

void vm_free(PTEntriesPtr pgdir) {
    vmem.vm_free(pgdir);
}

int uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
    return vmem.uvm_map(pgdir, va, sz, pa);
}

void uvm_switch(PTEntriesPtr pgdir) {
    // FIXME: Use NG and ASID for efficiency.
    arch_set_ttbr0(K2P(pgdir));
}


/*
 * generate a empty page as page directory
 */

static PTEntriesPtr my_pgdir_init() {
    /* TODO: Lab2 memory*/
    return (PTEntriesPtr)MmAllocatePhysicalPage();
}


/*
 * return the address of the pte in user page table
 * pgdir that corresponds to virtual address va.
 * if alloc != 0, create any required page table pages.
 */

static PTEntriesPtr my_pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {
    /* TODO: Lab2 memory*/
    PTEntriesPtr ret = MmiGetPageEntry(pgdir, vak);
    if (ret == NULL)
    {
        if (alloc)
        {
            // DON'T imitate this trick!
            MEMORY_SPACE m;
            KeInitializeSpinLock(&m.Lock);
            m.PageTable = pdgir;
            MmMapPageEx(&m, vak, VPTE_VALID);
            return MmiGetPageEntry(pgdir, vak);
        }
        else
        {
            return NULL;
        }
    }
    return ret;
}


/* Free a user page table and all the physical memory pages. */

void my_vm_free(PTEntriesPtr pgdir) {
    /* TODO: Lab2 memory*/
    MmiFreeTable(pgdir, 0);
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */

int my_uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
    /* TODO: Lab2 memory*/
    // DON'T imitate this trick!
    MEMORY_SPACE m;
    KeInitializeSpinLock(&m.Lock);
    m.PageTable = pdgir;
    for (void* a = va; a < va + sz; a += PAGE_SIZE)
    {
        if (!KSUCCESS(MmMapPageEx(&m, a, pa | PTE_USER_DATA)))
            return -1;
        pa += PAGE_SIZE;
    }
    return 0;
}

void virtual_memory_init(VMemory *vmem_ptr) {
    vmem_ptr->pgdir_init = my_pgdir_init;
    vmem_ptr->pgdir_walk = my_pgdir_walk;
    vmem_ptr->vm_free = my_vm_free;
    vmem_ptr->uvm_map = my_uvm_map;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void vm_test() {
    /* TODO: Lab2 memory*/
    PTEntriesPtr pg = pgdir_init();
    for (ULONG64 i = 0; i < 100000; i++)
    {
        PVOID p = MmAllocatePhysicalPage();
        my_uvm_map(pg, (PVOID)(i << 12), 4096, K2P(p));
        *(int*)p = i;
    }
    uvm_switch(pg);
    for (ULONG64 i = 0; i < 100000; i++)
    {
        if (*(int*)(P2K(PTE_ADDRESS(*my_pgdir_walk(pg, (PVOID)(i << 12), 1)))) != i ||
            *(int*)(i << 12) != i)
        {
            uart_put_char('x');
            for (;;);
        }
    }
    vm_free(pg);
    // Certify that your code works!
}