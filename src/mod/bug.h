#ifndef __BUG_H__
#define __BUG_H__

#include <common/lutil.h>
#include <ob/proc.h>

#define BUG_PANIC 0x23333333
#define BUG_STOP 0xdeedbeef
#define BUG_EXCEPTION 0x1

void KeBugFaultEx(CPCHAR, ULONG64, ULONG64);
#define KeBugFault(id) KeBugFaultEx(__FILE__, __LINE__, id)

#endif