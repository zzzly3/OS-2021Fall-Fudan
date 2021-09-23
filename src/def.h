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

static inline void putdec(const int n)
{
	char s[12];
	KSTRING ks;
	itos(n, s, 10);
	LibInitializeKString(&ks, s, 11);
	HalWriteConsoleString(&ks);
}

static inline void puthex(const int n)
{
	char s[12];
	KSTRING ks;
	itos(n, s, 16);
	LibInitializeKString(&ks, s, 11);
	HalWriteConsoleString(&ks);
}
