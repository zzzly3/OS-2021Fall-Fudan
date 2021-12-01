#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <driver/uart.h>
#include <common/lutil.h>
#include <ob/dev.h>

extern const PDEVICE_OBJECT HalConsoleDevice;

KSTATUS HalInitializeConsole();
KSTATUS HalWriteConsoleString(PKSTRING);
KSTATUS HalReadConsoleChar(PCHAR);
KSTATUS HalWriteConsoleChar(CHAR);

#endif