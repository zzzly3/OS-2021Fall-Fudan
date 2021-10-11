#include <aarch64/intrinsic.h>
#include <driver/console.h>
#include <ob/mem.h>
#include <common/variadic.h>

#define putchar HalWriteConsoleChar

static inline void system_init()
{
	HalInitializeDeviceManager();
	HalInitializeMemoryManager();
	HalInitializeConsole();
}

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

// Warning: This implementation is UNSAFE
static inline void printf(const char *fmt, ...) {
	char s[256];
	int p = 0;
    va_list arg;
    va_start(arg, fmt);
    while (*fmt && p < 239)
    	if (*fmt == '%') switch (*++fmt)
    	{
    		#define __CASE(c, t, b) case c: p += itos(va_arg(arg, t), &s[p], b); fmt++; break;
			__CASE('u', unsigned, 10)
			__CASE('d', int, 10)
			__CASE('x', unsigned, 16)
			__CASE('p', ULONG64, 16)
			__CASE('l', long long, 10)
			#undef __CASE
			case '\0': break;
			default: goto put_char;
		}
    	else put_char:
    		s[p++] = *fmt++;
    va_end(arg);
    s[p] = 0;
    putstr(s);
}
