#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <driver/uart.h>
#include <common/lutil.h>
#include <obj/dev.h>

extern const PDEVICE_OBJECT HalConsoleDevice;

KSTATUS HalInitializeConsole();
KSTATUS HalWriteConsoleString(PKSTRING outString);
KSTATUS HalReadConsoleChar(PCHAR inChar);
KSTATUS HalWriteConsoleChar(CHAR outChar);

#endif