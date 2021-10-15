#ifndef __DEF_H__
#define __DEF_H__

#include <aarch64/intrinsic.h>
#include <common/variadic.h>
#include <driver/console.h>
#include <ob/mem.h>
#include <ob/proc.h>
#include <mod/scheduler.h>

static inline void system_init()
{
	HalInitializeDeviceManager();
	HalInitializeMemoryManager();
	HalInitializeConsole();
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