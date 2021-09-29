#include <core/physical_memory.h>
#include <core/virtual_memory.h>
#include <def.h>

NORETURN void main() {
    ObInitializeDeviceManager();
    init_console();
    puts("Hello world!");
    //init_memory_manager();
    //init_virtual_memory();

}
