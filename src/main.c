#include <def.h>

NORETURN void main() {
    init_console();
	/* TODO: Lab1 print */
    puts("Hello world!");
    while (1)
    {
        char ch = getchar();
        putdec(ch);
        putchar(' ');
        puthex(ch);
        putchar(' ');
    }
}
