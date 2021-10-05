#include <def.h>
#include <core/char_device.h>

MEMORY_SPACE m;

PVOID MmAllocatePhysicalPage();

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    if (!MmInitializeMemorySpace(&m))
        putchar('x');
    char* p = (char*)MmAllocatePhysicalPage();
    if (!MmMapPageEx(&m, (PVOID)0xdeedbeef, (ULONG64)p | PTE_USER_DATA))
        putchar('x');
    MmSwitchMemorySpaceEx(NULL, &m);
    for (int i = 0; i < 4096; i++)
    {
        p[i] = "0123456789abcdef"[i & 15];
    }
    for (char* p = (char*)0xdeedbeef; p < (char*)0xdeedbeff; p++)
        putchar(*p);

}
