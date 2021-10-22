#ifndef __BUG_H__
#define __BUG_H__

#include <common/lutil.h>

void KeBugFaultEx(CPCHAR, ULONG64, ULONG64);
#define KeBugFault(id) KeBugFaultEx(__FILE__, __LINE__, id)

#endif