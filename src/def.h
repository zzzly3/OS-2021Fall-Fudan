#ifndef __DEF_H__
#define __DEF_H__

#include <aarch64/intrinsic.h>
#include <common/variadic.h>
#include <driver/console.h>
#include <ob/mem.h>
#include <ob/proc.h>
#include <ob/grp.h>
#include <ob/mutex.h>
#include <mod/bug.h>
#include <mod/scheduler.h>
#include <mod/worker.h>

void create_system_process();
// static void hello()
// {
// 	uart_put_char('h');
// 	reset_clock(1000);
// }

static inline void system_init()
{
	// Global operations
	// TODO: Replace with lock
	if (cpuid() == START_CPU)
	{
		extern char edata[], end[];
	    memset(edata, 0, end - edata);
	    HalInitializeMemoryManager();
		HalInitializeDeviceManager();
		HalInitializeConsole();
	}
	else // Wait the start cpu to complete initializing.
		delay_us(200*1000);
	// Per CPU operations
	ASSERT(ObInitializeProcessManager(), BUG_STOP);
	arch_enable_trap();
	if (cpuid() == START_CPU)
		create_system_process();
	// uart_put_char('h');
}

static inline void driver_init()
{
	// Initialize drivers & devices
	puts("Basic core loaded.");
	puts("Loading drivers...");
	putstr("sd_card...");
	sd_init();
	puts("done.");
}

#define putchar HalWriteConsoleChar
void puts(const char*);
void putstr(const char*);
void putdec(const long long);
void puthex(const long long);
char getchar();
int printf(const char*, ...);

#endif