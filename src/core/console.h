#pragma once

#ifndef _CORE_CONSOLE_H_
#define _CORE_CONSOLE_H_

#define NEWLINE '\n'

#ifdef USE_LAGACY

    #include <common/spinlock.h>
    #include <common/variadic.h>
    #include <core/char_device.h>

    typedef struct {
        SpinLock lock;
        CharDevice device;
    } ConsoleContext;

    void init_console();

    void puts(const char *str);
    void vprintf(const char *fmt, va_list arg);
    void printf(const char *fmt, ...);

    NORETURN void _panic(const char *file, usize line, const char *fmt, ...);

    #define PANIC(...) _panic(__FILE__, __LINE__, __VA_ARGS__)

#else

    #include <def.h>
    #define PANIC(...) {printf(__VA_ARGS__); KeBugFault(0x23333333); }

#endif

#define assert(predicate)                                                                          \
    do {                                                                                           \
        if (!(predicate))                                                                          \
            PANIC("assertion failed: \"%s\"", #predicate);                                         \
    } while (false)

#define asserts(predicate, ...)                                                                    \
    do {                                                                                           \
        if (!(predicate))                                                                          \
            PANIC("assertion failed: \"%s\". %s", #predicate, __VA_ARGS__);                          \
    } while (false)

#endif
