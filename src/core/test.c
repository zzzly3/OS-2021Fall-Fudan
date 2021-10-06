#include <def.h>
#include <core/virtual_memory.h>

#define sys_test_pass(message) {puts(message);}
#define sys_test_fail(message) {puts(message);while(1);}

void sys_test()
{
    puts("Memory");
    init_virtual_memory();
	sys_mem_test();
}

PVOID MmAllocatePhysicalPage();
void sys_mem_test()
{
	static MEMORY_SPACE m;
	int p0 = MmGetAllocatedPagesCount();
    vm_test();
    puts("Pass: legacy");
	MmInitializeMemorySpace(&m);
	ULONG64 j = 0;
    for (ULONG64 i = 0; i < 1000000; i++)
    {
        int* p = MmAllocatePhysicalPage();
        if (p == NULL)
        {
            j = i;
            break;
        }
        KSTATUS ret = MmMapPageEx(&m, (PVOID)(i << 12), (ULONG64)p | PTE_USER_DATA);
        if (!KSUCCESS(ret))
        {
            j = i;
            break;
        }
        *(p + (i & 1023)) = i;
    }
    sys_test_pass("Pass: allocate");
    MmSwitchMemorySpaceEx(NULL, &m);
    for (ULONG64 i = 0; i < j; i++)
    {
        if (*((int*)(i << 12) + (i & 1023)) != i)
        	sys_test_fail("Fail: read");
        *((int*)(i << 12) + (i & 1023)) = i + 1;
    }
    sys_test_pass("Pass: read");
    for (ULONG64 i = 0; i < j; i++)
    {
    	if (*(int*)MmGetPhysicalAddressEx(&m, (PVOID)((int*)(i << 12) + (i & 1023))) != i + 1)
    		sys_test_fail("Fail: write");
    }
    sys_test_pass("Pass: write");
    MmDestroyMemorySpace(&m);
    if (MmGetAllocatedPagesCount() != p0)
    	sys_test_fail("Fail: destroy");
    sys_test_pass("Pass: destroy");
}