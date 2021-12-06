#ifndef __BUG_H__
#define __BUG_H__

#include <common/lutil.h>
#include <ob/proc.h>

#define BUG_PANIC 0x23333333
#define BUG_STOP 0xdeedbeef
#define BUG_CHECKFAIL 0x11111111
#define BUG_EXCEPTION 0x1
#define BUG_BADLEVEL 0x2
#define BUG_BADLOCK 0x3
#define BUG_BADDPC 0x4
#define BUG_UNSAFETRAP 0x5
#define BUG_SCHEDULER 0x6
#define BUG_BADREF 0x7
#define BUG_FSFAULT 0x8
#define BUG_WORKER 0x9
#define BUG_BADIO 0xa

void KeBugFaultEx(CPCHAR, ULONG64, ULONG64);
#define KeBugFault(id) KeBugFaultEx(__FILE__, __LINE__, id)
#define ASSERT(expr, id) if (!(expr)) KeBugFault(id)

#endif