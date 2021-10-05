#include <def.h>
#include <core/char_device.h>

MEMORY_SPACE m;

PVOID MmAllocatePhysicalPage();

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    MmInitializeMemorySpace(&m);
    for (ULONG64 i = 0; i < 1000000; i++)
    {
        int* p = MmAllocatePhysicalPage();
        KSTATUS ret = MmMapPageEx(&m, (PVOID)(i << 12), (ULONG64)p | PTE_USER_DATA);
        if (!KSUCCESS(ret))
        {
            putdec(i);
            putchar('x');
            break;
        }
        *p = i;
        if (i % 10000 == 0) putchar('a');
    }
    for (ULONG64 i = 0; i < 1000000; i++)
    {
        ULONG64 p = (ULONG64)MmGetPhysicalAddressEx(&m, (PVOID)(i << 12));
        int j = *(int*)p;
        if (j != i)
        {
            puthex(p);
            putchar(':');
            putdec(i);
            putchar('=');
            putdec(j);
            putchar('X');
            break;
        }
    }
    putchar('-');
    putdec(MmGetAllocatedPagesCount());
    putchar('\n');
    MmDestroyMemorySpace(&m);
    putdec(MmGetAllocatedPagesCount());
    putchar('\n');
    MmInitializeMemorySpace(&m);
    for (ULONG64 i = 1000000; i > 0; i--)
    {
        int* p = MmAllocatePhysicalPage();
        KSTATUS ret = MmMapPageEx(&m, (PVOID)(i << 12), (ULONG64)p | PTE_USER_DATA);
        if (!KSUCCESS(ret))
        {
            putdec(i);
            putchar('x');
            break;
        }
        *p = i;
        if (i % 10000 == 0) putchar('a');
    }
    for (ULONG64 i = 1000000; i > 0; i--)
    {
        ULONG64 p = (ULONG64)MmGetPhysicalAddressEx(&m, (PVOID)(i << 12));
        int j = *(int*)p;
        if (j != i)
        {
            puthex(p);
            putchar(':');
            putdec(i);
            putchar('=');
            putdec(j);
            putchar('X');
            break;
        }
    }
    putchar('-');
    putdec(MmGetAllocatedPagesCount());
    putchar('\n');
}
