#include <def.h>

NORETURN void main() {
    init_char_device();
    init_console();
	/* TODO: Lab1 print */
    puts("Hello world!");
    while (1) put_char(get_char());
}
