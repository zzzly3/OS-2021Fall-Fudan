#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <common/lutil.h>
#include <common/defines.h>
#include <ob/proc.h>

void ObInitializeScheduler();
void KeRaiseRealTimeState(BOOL*);
void KeLowerRealTimeState(BOOL);
void KeTaskSwitch();

#endif