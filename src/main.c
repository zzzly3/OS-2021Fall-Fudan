#include <def.h>

NORETURN void main() {
    init_console();
	/* TODO: Lab1 print */
    puts("Hello world!");
    while (1)
    {
        char ch = getchar();
        if (ch == '\r')
            putchar('\n');
        else if (ch == 0x7f)
            putchar('\b'),putchar('\0');
        else
            putchar(ch);
    }
}
