#include <def.h>
#include <core/char_device.h>

MEMORY_SPACE m;

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    if (MmInitializeMemorySpace(&m))
        puthex((ULONG64)m.ttbr0);
    else
        putchar('x');
}
