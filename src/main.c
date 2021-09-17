#include <aarch64/intrinsic.h>
#include <core/console.h>

NORETURN void main() {
    init_char_device();
    init_console();
	/* TODO: Lab1 print */
    puts("Hello world!");
    puts("EEEEEchoooo!");
    for (;;)
    {
        char ch = uart_get_char();
        if (ch == -1)
            continue;
        uart_put_char(ch);
    }
}
