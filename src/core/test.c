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
    //sys_test_info("Test memory");
	//sys_mem_test();
    sys_test_info("Test process");
    sys_switch_test();
    sys_test_info("Finish");
    KeBugFault(BUG_STOP);
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

static MUTEX mut, mut2;
static int a[4096], cnt;
static int chk;
static int core1[CPU_NUM], core2[CPU_NUM];
void sys_switch_test_proc(ULONG64 arg)
{
    int n;
    switch (arg)
    {
        case 0: {
            KeRaiseExecuteLevel(EXECUTE_LEVEL_APC);
            if (!KSUCCESS(KeWaitForMutexSignaled(&mut, FALSE)))
                KeBugFault(BUG_STOP);
            KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
            // printf("#1 proc %d run in cpu %d\n", n, cpuid());
            chk = PsGetCurrentProcess()->ProcessId;
            int sum;
            for (int i = 0; i < 4096; i++)
            {
                sum = 0;
                for (int j = 0; j < 4096; j++)
                {
                    sum = (sum + a[j]) % 19260817;
                }
                a[i] = (a[i] + (i ^ sum)) % 19260817;
            }
            n = cnt++;
            if (chk != PsGetCurrentProcess()->ProcessId)
                KeBugFault(BUG_CHECKFAIL);
            if (cnt > 100)
                KeBugFault(BUG_CHECKFAIL);
            if (cnt == 100)
                KeCreateDpc(sys_switch_test_proc, 2);
            core1[cpuid()]++;
            KeSetMutexSignaled(&mut);
        }
        case 1 : {
            KeRaiseExecuteLevel(EXECUTE_LEVEL_APC);
            if (!KSUCCESS(KeWaitForMutexSignaled(&mut2, TRUE)))
                KeBugFault(BUG_STOP);
            KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
            //printf("#2 proc %d run in cpu %d\n", n, cpuid());
            for (int i = 0; i < 40; i++)
            {
                for (int j = 0; j < a[n * 40 + i]; j++)
                {
                    a[n * 40 + i] = ((long long)a[n * 40 + i] * a[n * 40 + i] + chk) % 19260817;
                    delay_us(1);
                }
            }
            KeRaiseExecuteLevel(EXECUTE_LEVEL_APC);
            if (!KSUCCESS(KeWaitForMutexSignaled(&mut, FALSE)))
                KeBugFault(BUG_STOP);
            KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
            cnt++;
            if (cnt > 200)
                KeBugFault(BUG_CHECKFAIL);
            if (cnt == 200)
                KeCreateDpc(sys_switch_test_proc, 3);
            //printf("#3 proc %d run in cpu %d\n", n, cpuid());
            core2[cpuid()]++;
            KeSetMutexSignaled(&mut);
        } break;
        case 2 : {
            int sum = 0;
            for (int i = 0; i < 4096; i++)
            {
                sum = (sum + a[i]) % 19260817;
            }
            printf("ans: %d\n", sum);
            if (sum == 6007406)
                sys_test_pass("Pass: serial")
            else
                sys_test_fail("Fail: serial")
            sys_test_info("Parallel 100 Process")
            chk = 12345;
            KeSetMutexSignaled(&mut2);
        } return;
        case 3 : {
            int sum = 0;
            for (int i = 0; i < 4000; i++)
            {
                sum = (sum + a[i]) % 19260817;
            }
            printf("ans: %d\n", sum);
            if (sum == 16268470)
                sys_test_pass("Pass: parallel")
            else
                sys_test_fail("Fail: parallel")
            printf("Core balance: #1 [%d %d %d %d]; #2 [%d %d %d %d]\n", core1[0], core1[1], core1[2], core1[3], core2[0], core2[1], core2[2], core2[3]);
            // KeCreateDpc(sys_switch_test_proc, 4);
        } return;
        case 4: {
            delay_us(100 * 1000);
            PKPROCESS p;
            ASSERT(KSUCCESS(PsReferenceProcessById(100, &p)), BUG_STOP);
            ASSERT(p->ProcessId == 100, BUG_CHECKFAIL);
            printf("%d\n", p->Status);
        } return;
    }
    KeExitProcess();
}

void sys_switch_test()
{
    delay_us(TIME_SLICE_MS * 2);
    sys_test_info("Serial 100 Process")
    KeInitializeMutex(&mut);
    KeInitializeMutex(&mut2);
    mut2.Signaled = FALSE;
    int pid[100]; // 25 * 4
    for (int i = 0; i < 100; i++)
    {
        KeCreateProcess(NULL, sys_switch_test_proc, 0, &pid[i]);
    }
}

extern int ActiveProcessCount[CPU_NUM];
static int cntt;
void sys_transfer_test_proc(ULONG64 arg)
{
    if (arg == 23333)
    {
        for (int i = 0; i < CPU_NUM; i++)
            ASSERT(ActiveProcessCount[i] == 1, BUG_CHECKFAIL);
        sys_test_pass("Pass: transfer");
        // delay_us(1000 * 1000);
        return;
    }
    delay_us(arg * 1000 * 1000);
    printf("CPU %d, Process %d (%d %d %d %d)\n", cpuid(), arg, ActiveProcessCount[0], ActiveProcessCount[1], ActiveProcessCount[2], ActiveProcessCount[3]);
    if (++cntt == 25)
    {
        KeCreateDpc(sys_transfer_test_proc, 23333);
    }
}
void sys_transfer_test()
{
    sys_test_info("Process transfer test");
    int pid[25];
    for (int i = 0; i < 25; i++)
    {
        KeCreateProcess(NULL, sys_transfer_test_proc, i, &pid[i]);
    }
}

void sys_group_test_proc(int arg)
{
    arch_disable_trap();
    PKPROCESS cur = PsGetCurrentProcess();
    if (arg < 0)
    {
        for (int i = 0; i < 4; i++)
        {
            printf("Outter process: pid=%d gpid=%d group=%p\n", cur->ProcessId, cur->GroupProcessId, cur->Group);
            KeTaskSwitch();
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            printf("Inner process %d: pid=%d gpid=%d group=%p\n", arg, cur->ProcessId, cur->GroupProcessId, cur->Group);
            KeTaskSwitch();
        }
    }
}
void sys_group_test()
{
    sys_test_info("Process group test");
    PPROCESS_GROUP g = PgCreateGroup();
    PKPROCESS p[4];
    for (int i = 0; i < 4; i++)
    {
        p[i] = PsCreateProcessEx();
        ASSERT(p[i], BUG_STOP);
        p[i]->Flags |= PROCESS_FLAG_KERNEL;
        p[i]->Group = g;
        PsCreateProcess(p[i], sys_group_test_proc, i);
        ObDereferenceObject(p[i]);
    }
    ObDereferenceObject(g);
    int pid;
    KeCreateProcess(NULL, sys_group_test_proc, -1, &pid);
}
