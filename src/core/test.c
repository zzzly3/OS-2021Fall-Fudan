#include <def.h>
#include <core/virtual_memory.h>

#define sys_test_pass(message) {puts(message);}
#define sys_test_fail(message) {puts(message);while(1);}

void sys_mem_test();

void sys_test()
{
    //puts("Memory");
	//sys_mem_test();
    puts("Switch");
    sys_switch_test();
}

void sys_mem_test()
{
    init_virtual_memory(); // lagacy
	PMEMORY_SPACE m;
	int p0 = MmGetAllocatedPagesCount();
    vm_test();
    puts("Pass: legacy");
	m = MmCreateMemorySpace();
	ULONG64 j = 0;
    for (ULONG64 i = 0; i < 1000000; i++)
    {
        int* p = MmAllocatePhysicalPage();
        if (p == NULL)
        {
            j = i;
            break;
        }
        KSTATUS ret = MmMapPageEx(m, (PVOID)(i << 12), (ULONG64)p | PTE_USER_DATA);
        if (!KSUCCESS(ret))
        {
            j = i;
            break;
        }
        *(p + (i & 1023)) = i;
    }
    sys_test_pass("Pass: allocate");
    MmSwitchMemorySpaceEx(NULL, m);
    for (ULONG64 i = 0; i < j; i++)
    {
        if (*((int*)(i << 12) + (i & 1023)) != i)
        	sys_test_fail("Fail: read");
        *((int*)(i << 12) + (i & 1023)) = i + 1;
    }
    sys_test_pass("Pass: read");
    for (ULONG64 i = 0; i < j; i++)
    {
    	if (*(int*)MmGetPhysicalAddressEx(m, (PVOID)((int*)(i << 12) + (i & 1023))) != i + 1)
    		sys_test_fail("Fail: write");
    }
    sys_test_pass("Pass: write");
    MmDestroyMemorySpace(m);
    if (MmGetAllocatedPagesCount() != p0)
    	sys_test_fail("Fail: balance");
    sys_test_pass("Pass: balance");
}

extern PKPROCESS KernelProcess;
static PKPROCESS NewProcess;
static int pt = 0;
void swtch (PVOID kstack, PVOID* oldkstack);
void sys_switch_test_proc()
{
    for (int i = 1; i <= 10; i++)
    {
        printf("New process message #%d", i);
        pt = i;
        swtch(KernelProcess->Context.KernelStack.p, &NewProcess->Context.KernelStack.p);
    }
}
void sys_switch_test()
{
    NewProcess = PsCreateProcessEx();
    if (NewProcess == NULL) sys_test_fail("Fail: switch");
    NewProcess->Context.KernelStack.d->lr = (ULONG64)sys_switch_test_proc;
    for (int i = 0; i < 10; i++)
    {
        swtch(NewProcess->Context.KernelStack.p, &KernelProcess->Context.KernelStack.p);
    }
    if (pt == 10) sys_test_pass("Pass: switch") else sys_test_fail("Fail: switch");
}


