#include <core/sched.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/virtual_memory.h>
#include <mod/scheduler.h>

struct {
    struct proc proc[NPROC];
} ptable;

static void scheduler_simple();
static struct proc *alloc_pcb_simple();
static void sched_simple();
struct sched_op simple_op = {
    .scheduler = scheduler_simple, .alloc_pcb = alloc_pcb_simple, .sched = sched_simple};
struct scheduler simple_scheduler = {.op = &simple_op};

int nextpid = 1;
void swtch(struct context **, struct context *);
/*
 * Per-CPU process scheduler
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns.  It loops, doing:
 *  - choose a process to run
 *  - swtch to start running that process
 *  - eventually that process transfers control
 *        via swtch back to the scheduler.
 */
static void scheduler_simple() {
    struct proc *p;
    struct cpu *c = thiscpu();
    c->proc = NULL;

    for (;;) {
        /* Loop over process table looking for process to run. */
        /* TODO: Lab3 Schedule */
        // Schedulers are spawned in RT level by default, thus task-switching can be directly invoked.
        // If used in other ways, hope you remember to raise the execute-level...
        RT_ONLY
            KeTaskSwitch();
    }
}

/*
 * `Swtch` to thiscpu->scheduler.
 */
static void sched_simple() {
    /* TODO: Lab3 Schedule */
    RT_ONLY
	   KeTaskSwitch();
}

/* 
 * Allocate an unused entry from ptable.
 * Allocate a new pid for it.
 */
static struct proc *alloc_pcb_simple() {
    /* TODO: Lab3 Schedule */
    // Not used. Just make the TA happy.
    PKPROCESS pp = PsCreateProcessEx();
    return (struct proc*)((ULONG64)&pp->ProcessId - (ULONG64)&((struct proc*)0)->pid);
}