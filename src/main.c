#include <def.h>
#include <core/char_device.h>

void sys_mem_test();

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    puts("Memory test");
    sys_mem_test();
    puts("Again");
    sys_mem_test();

}
