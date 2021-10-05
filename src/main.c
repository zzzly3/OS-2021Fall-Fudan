#include <def.h>
#include <core/char_device.h>

MEMORY_SPACE m;

NORETURN void main() {
    fuck_gcc(); // FUCK THE LINKER!
    system_init();
    
    if (!MmInitializeMemorySpace(&m))
        putchar('x');
    putdec(MmGetAllocatedPagesCount());
    MmDestroyMemorySpace(&m);
    if (!MmInitializeMemorySpace(&m))
        putchar('x');
    putdec(MmGetAllocatedPagesCount());
    putchar('p');


}
