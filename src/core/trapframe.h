#pragma once

#ifndef _CORE_TRAPFRAME_H_
#define _CORE_TRAPFRAME_H_

#include <common/defines.h>

typedef struct {
	/* TODO: Lab3 Interrupt */
    uint64_t reserved[16]; // See TRAP_FRAME
} Trapframe;

#endif
