#include <mem.h>
#include <def.h>

extern DEVICE_OBJECT RootDeviceX;

NORETURN void main() {
    init_uart_device();
    init_console();
    puts("Hello world!");
    //init_memory_manager();
    //init_virtual_memory();
    HalInitializeMemoryManager();
    MEMORY_SPACE m;
    if (MmInitializeMemorySpace(&m))
        puthex(m.ttbr0);
    else
        putchar('x');
}
