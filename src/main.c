#include <aarch64/intrinsic.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/virtual_memory.h>
#include <def.h>

NORETURN void main() {
    init_console();
	/* TODO: Lab1 print */
    puts("hello! world!");
    puthex((ULONG64)&main);
    puts(" (main)");
    int t;
    puthex((ULONG64)&t);
    puts(" (stack)");
    init_memory_manager();
    init_virtual_memory();

}
