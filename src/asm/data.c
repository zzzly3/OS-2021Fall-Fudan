#include <core/trap.h>
#include <core/proc.h>

u64 __attribute__((aligned(16))) TrapContext[CPU_NUM][16];