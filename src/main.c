#include <core/physical_memory.h>
#include <core/virtual_memory.h>
#include <def.h>

extern DEVICE_OBJECT RootDeviceX;

NORETURN void main() {
    init_char_device();
    init_console();
    puts("Hello world!");
    //init_memory_manager();
    //init_virtual_memory();
    KSTRING x;
    puthex((ULONG64)&RootDeviceX);
    putchar(' ');
    puthex((ULONG64)HalConsoleDevice->DeviceList.Backward);
    putchar('\n');
    LibInitializeKString(&x, "console", 16);
    puthex((ULONG64)IouLookupDevice(&x));
    putchar('\n');
    puthex((ULONG64)HalConsoleDevice);
    putchar('\n');
    LibInitializeKString(&x, "consolex", 16);
    puthex((ULONG64)IouLookupDevice(&x));
    putchar('\n');
}
