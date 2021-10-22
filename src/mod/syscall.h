#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/lutil.h>
#include <ob/proc.h>
#include <core/syscall.h>

void KiSystemCallEntry(PTRAP_FRAME);

#endif