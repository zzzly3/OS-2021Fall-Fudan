#include <def.h>

#define sys_test_pass(message) {puts(message);}
#define sys_test_fail(message) {puts(message);while(1);}

void sys_test()
{
	return;
}

void sys_mem_test()
{
	static MEMORY_SPACE m;
	int p0 = MmGetAllocatedPagesCount();
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
        *p = i;
    }
    sys_test_pass("Pass: allocate");
    MmSwitchMemorySpaceEx(NULL, &m);
    for (ULONG64 i = 0; i < j; i++)
    {
        if (*(int*)(i << 12) != i)
        	sys_test_fail("Fail: read");
        *(int*)(i << 12) = i + 1;
    }
    sys_test_pass("Pass: read");
    for (ULONG64 i = 0; i < j; i++)
    {
    	if (*(int*)MmGetPhysicalAddressEx(&m, (PVOID)(i << 12)) != i + 1)
    		sys_test_fail("Fail: write");
    }
    sys_test_pass("Pass: write");
    MmDestroyMemorySpace(&m);
    if (MmGetAllocatedPagesCount() != p0)
    	sys_test_fail("Fail: balance");
    sys_test_pass("Pass: balance");
}