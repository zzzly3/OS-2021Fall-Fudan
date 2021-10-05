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
        MmMapPageEx(&m, (PVOID)(i << 12), (ULONG64)p | PTE_USER_DATA);
        *p = i;
    }
    for (ULONG64 i = 0; i < 1000000; i++)
    {
        if (*(int*)MmGetPhysicalAddressEx(&m, (PVOID)(i << 12)) != i)
            putchar('x');
    }
    putchar('p');
    MmDestroyMemorySpace(&m);
    putdec(MmGetAllocatedPagesCount());
    MmInitializeMemorySpace(&m);
    for (ULONG64 i = 1000000; i > 0; i--)
    {
        int* p = MmAllocatePhysicalPage();
        MmMapPageEx(&m, (PVOID)(i << 12), (ULONG64)p | PTE_USER_DATA);
        *p = i;
    }
    for (ULONG64 i = 1000000; i > 0; i--)
    {
        if (*(int*)MmGetPhysicalAddressEx(&m, (PVOID)(i << 12)) != i)
            putchar('x');
    }
    putchar('p');
}
