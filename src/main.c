#include <core/physical_memory.h>
#include <core/virtual_memory.h>
#include <def.h>

NORETURN void main() {
    init_char_device();
    init_console();
    puts("Hello world!");
    //init_memory_manager();
    //init_virtual_memory();
    KSTRING x;
    LibInitializeKString(&x, "console", 16);
    puthex((ULONG64)IouLookupDevice(&x));
    putchar('\n');
    puthex((ULONG64)HalConsoleDevice);
    putchar('\n');
    LibInitializeKString(&x, "consolex", 16);
    puthex((ULONG64)IouLookupDevice(&x));
    putchar('\n');
}
