#include <def.h>
#include <core/virtual_memory.h>
#include <core/proc.h>

#define sys_test_pass(message) {puts("\033[42;30m"message"\033[0m");}
#define sys_test_fail(message) {puts("\033[41;30m"message"\033[0m");while(1);}
#define sys_test_info(message) {puts("\033[43;34m"message"\033[0m");}

void sys_switch_test();
void sys_mem_test();

void sys_test()
{
    sys_test_info("Test memory");
	sys_mem_test();
    sys_test_info("Test process");
    sys_switch_test();
    sys_test_info("Finish");
    while(1);
}

void sys_mem_test()
{
    init_virtual_memory(); // lagacy
	PMEMORY_SPACE m;
	int p0 = MmGetAllocatedPagesCount();
    vm_test();
    sys_test_pass("Pass: legacy");
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
        KSTATUS ret = MmMapPageEx(m, (PVOID)(i << 12), (ULONG64)K2P(p) | PTE_USER_DATA);
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
static pt = 0;
void swtch (PVOID kstack, PVOID* oldkstack);
void sys_switch_callback(ULONG64 arg)
{
    pt++;
    printf("CB: arg = %d, pid = %d\n", arg, PsGetCurrentProcess()->ProcessId);
}
void sys_switch_test_proc(ULONG64 arg)
{
    PKPROCESS current = PsGetCurrentProcess();
    KeCreateApcEx(current, sys_switch_callback, arg);
    for(;;)
    {
        u64 p;
        asm volatile("mov %[x], sp" : [x] "=r"(p));
        printf("CPU %d, Process %d, pid = %d, stack at %p\n", cpuid(), arg, current->ProcessId, p);
        if (arg == 0)
        {
            printf("Process 0 exit.\n");
            KeExitProcess();
        }
        delay_us(1000 * 1000);
    }
}
void sys_switch_test()
{
    u64 p, p2;
    asm volatile("mov %[x], sp" : [x] "=r"(p));
    int pid[10];
    for (int i = 0; i < 3; i++)
    {
        if (!KSUCCESS(KeCreateProcess(NULL, (PVOID)sys_switch_test_proc, i, &pid[i])))
            sys_test_fail("Fail: create");
        printf("pid[%d]=%d\n", i, pid[i]);
    }
    sys_test_pass("Pass: create");
    KeCreateDpc(sys_switch_callback, 233);
    KeCreateDpc(sys_switch_callback, 2333);
    KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
    delay_us(3000 * 1000);
    if (pt == 4)
        sys_test_pass("Pass: switch");
    else
        sys_test_fail("Fail: switch")
    spawn_init_process();
    delay_us(1000 * 1000);
    KeRaiseExecuteLevel(EXECUTE_LEVEL_RT);
    sys_test_pass("Pass: init");
    asm volatile("mov %[x], sp" : [x] "=r"(p2));
    if (p == p2)
        sys_test_pass("Pass: balance")
    else
        sys_test_fail("Fail: balance")
}

