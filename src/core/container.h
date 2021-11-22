#pragma once

#include <common/spinlock.h>
#include <core/proc.h>
#include <core/sched.h>

#define NPID 64
struct pid_mapping {
    bool valid;
    int pid_local;
    struct proc *p;
};
typedef struct pid_mapping pid_mapping;

typedef enum { MEMORY, PID, INODE } resource_t;

struct container {
    struct proc *p;
    struct scheduler scheduler;
    SpinLock lock;
    struct container *parent;

    // pid
    struct pid_mapping pmap[NPID];
};

typedef struct container container;

#ifdef USE_LAGACY

extern struct container *root_container;

void init_container();

void *alloc_resource(struct container *this, struct proc *p, resource_t resource);

void trace_usage(struct container *this, struct proc *p, resource_t resource);

void container_test_init();

#else

#include <ob/proc.h>
#include <mod/scheduler.h>

#define root_container NULL

extern void add_loop_test(int times);
static inline void container_test_init()
{
    add_loop_test(1);
    PPROCESS_GROUP g = PgCreateGroup();
    KeCreateApcEx(g->GroupWorker, (PAPC_ROUTINE)add_loop_test, 8);
}

#endif