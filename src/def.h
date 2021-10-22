#ifndef __DEF_H__
#define __DEF_H__

#include <aarch64/intrinsic.h>
#include <common/variadic.h>
#include <driver/console.h>
#include <ob/mem.h>
#include <ob/proc.h>
#include <ob/mutex.h>
#include <mod/bug.h>
#include <mod/scheduler.h>

static void hello()
{
	uart_put_char('h');
	reset_clock(1000);
}

static inline void system_init()
{
	// Common operations
	if (cpuid() == 0)
	{
		extern char edata[], end[];
	    memset(edata, 0, end - edata);
		HalInitializeDeviceManager();
		HalInitializeMemoryManager();
		HalInitializeConsole();
	}
	delay_us(100 * 1000);
	uart_put_char('0' + cpuid());
	// Per CPU operations
	ObInitializeProcessManager();
	arch_enable_trap();
}

#define putchar HalWriteConsoleChar
void puts(const char*);
void putstr(const char*);
void putdec(const long long);
void puthex(const long long);
char getchar();
int printf(const char*, ...);

#endif