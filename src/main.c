#include <def.h>
#include <core/char_device.h>

void sys_test();

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    sys_test();

}
