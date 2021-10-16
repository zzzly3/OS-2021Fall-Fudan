#pragma once

#ifndef _CORE_TRAPFRAME_H_
#define _CORE_TRAPFRAME_H_

#include <common/defines.h>
#include <ob/proc.h>

typedef struct {
	/* TODO: Lab3 Interrupt */
    TRAP_FRAME tf;
    uint64_t reserved[16];
} Trapframe;

#endif
