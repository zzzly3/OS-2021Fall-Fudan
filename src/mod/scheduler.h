#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <common/lutil.h>
#include <common/defines.h>
#include <driver/clock.h>
#include <ob/proc.h>

#define TIME_SLICE_MS 100

void ObInitializeScheduler();
EXECUTE_LEVEL KeRaiseExecuteLevel(EXECUTE_LEVEL);
void KeLowerExecuteLevel(EXECUTE_LEVEL);
void KeTaskSwitch();

#endif