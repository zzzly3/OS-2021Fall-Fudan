#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/lutil.h>
#include <ob/mem.h>
#include <ob/proc.h>
#include <core/syscall.h>
#include <sys/syscall.h>

#define USPACE_TOP 0x0001000000000000

void KiSystemCallEntry(PTRAP_FRAME);
BOOL KiValidateBuffer(PVOID, unsigned);
unsigned KiValidateString(PVOID);
struct file* KiValidateFileDescriptor(int);

#endif