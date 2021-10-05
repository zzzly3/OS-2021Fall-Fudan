#include <def.h>
#include <core/char_device.h>

MEMORY_SPACE m;

PVOID MmAllocatePhysicalPage();

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    if (!MmInitializeMemorySpace(&m))
        putchar('x');
    puthex(MmGetPhysicalAddressEx(&m, 0xdeedbeef));
    putchar(' ');
    char* p = (char*)MmAllocatePhysicalPage();
    puthex((ULONG64)p);
    putchar(' ');
    if (!MmMapPageEx(&m, 0xdeedbeef, (ULONG64)p | PTE_USER_DATA))
        putchar('x');
    puthex(MmGetPhysicalAddressEx(&m, 0xdeedbeef));
    putchar('\n');
    puthex(MmGetAllocatedPagesCount());
    putchar('\n');
    if (!MmUnmapPageEx(&m, 0xdeedbeef))
        putchar('x');
    puthex(MmGetPhysicalAddressEx(&m, 0xdeedbeef));
    putchar('\n');

}
