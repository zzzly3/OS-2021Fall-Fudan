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

static inline void putstr(const char* s)
{
	KSTRING ks;
	if (LibInitializeKString(&ks, s, 256))
		HalWriteConsoleString(&ks);
}

static inline void putdec(const long long n)
{
	char s[24];
	KSTRING ks;
	itos(n, s, 10);
	LibInitializeKString(&ks, s, 23);
	HalWriteConsoleString(&ks);
}

static inline void puthex(const long long n)
{
	char s[24];
	KSTRING ks;
	itos(n, s, 16);
	LibInitializeKString(&ks, s, 23);
	HalWriteConsoleString(&ks);
}


static inline char getchar()
{
	char c;
	return KSUCCESS(HalReadConsoleChar(&c)) ? c : -1;
}
