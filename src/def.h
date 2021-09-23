#include <aarch64/intrinsic.h>
#include <driver/console.h>

#define init_console HalInitializeConsole

#define putchar HalWriteConsoleChar

static inline void puts(const char* s)
{
	KSTRING ks;
	if (LibInitializeKString(&ks, s, 256))
	{
		HalWriteConsoleString(&ks);
		HalWriteConsoleChar('\n');
	}
}

static inline char getchar()
{
	char c;
	return KSUCCESS(HalReadConsoleChar(&c)) ? c : -1;
}
