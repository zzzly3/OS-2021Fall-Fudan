#include <def.h>
#include <core/char_device.h>

extern DEVICE_OBJECT RootDeviceX;

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    init_device();
    init_console();
    puts("Hello world!");
    //init_memory_manager();
    //init_virtual_memory();
    HalInitializeMemoryManager();
    MEMORY_SPACE m;
    if (MmInitializeMemorySpace(&m))
        puthex((ULONG64)m.ttbr0);
    else
        putchar('x');
}
