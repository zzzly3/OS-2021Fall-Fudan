#include <aarch64/intrinsic.h>
#include <core/console.h>

NORETURN void main() {
    init_char_device();
    init_console();
	/* TODO: Lab1 print */
    puts("Hello world!");
    char ch = -1;
    while (ch == (char)-1) ch = uart_get_char();
    printf("%d\n", (int)ch);
}
