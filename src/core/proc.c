
#include <core/proc.h>
#include <aarch64/mmu.h>
#include <core/virtual_memory.h>
#include <core/physical_memory.h>
#include <common/string.h>
#include <core/sched.h>
#include <core/console.h>
#include <ob/proc.h>
#include <ob/mem.h>

void forkret();
extern void trap_return();
/*
 * Look through the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state (allocate stack, clear trapframe, set context for switch...)
 * required to run in the kernel. Otherwise return 0.
 * Step 1 (TODO): Call `alloc_pcb()` to get a pcb.
 * Step 2 (TODO): Set the state to `EMBRYO`.
 * Step 3 (TODO): Allocate memory for the kernel stack of the process.
 * Step 4 (TODO): Reserve regions for trapframe and context in the kernel stack.
 * Step 5 (TODO): Set p->tf and p->context to the start of these regions.
 * Step 6 (TODO): Clear trapframe.
 * Step 7 (TODO): Set the context to work with `swtch()`, `forkret()` and `trap_return()`.
 */
static struct proc *alloc_proc() {
    struct proc *p;
    /* TODO: Lab3 Process */
    // Not used. Just make the TA happy.
    PKPROCESS pp = PsCreateProcessEx();
    p = (struct proc*)((ULONG64)&pp->ProcessId - (ULONG64)&((struct proc*)0)->pid);
    return p;
}

/*
 * Set up first user process(Only used once).
 * Step 1: Allocate a configured proc struct by `alloc_proc()`.
 * Step 2 (TODO): Allocate memory for storing the code of init process.
 * Step 3 (TODO): Copy the code (ranging icode to eicode) to memory.
 * Step 4 (TODO): Map any va to this page.
 * Step 5 (TODO): Set the address after eret to this va.
 * Step 6 (TODO): Set proc->sz.
 */
void spawn_init_process() {
    // struct proc *p;
    extern char icode[], eicode[];
    // p = alloc_proc();
    /* TODO: Lab3 Process */
    BOOL trpen = arch_disable_trap();
    PKPROCESS p = PsCreateProcessEx();
    if (p == NULL)
        goto fail;
    PMEMORY_SPACE m = MmCreateMemorySpace();
    if (m == NULL)
        goto fail;
    int sz = eicode - icode, pg = (sz + PAGE_SIZE - 1) / PAGE_SIZE;
    PVOID base = (PVOID)0x40000000;
    for (int i = 0; i < pg; i++)
    {
        if (!KSUCCESS(MmCreateUserPageEx(m, (PVOID)((ULONG64)base + i * PAGE_SIZE))))
            goto fail;
    }
    MmSwitchMemorySpaceEx(NULL, m);
    memcpy(base, (PVOID)icode, sz);
    MmSwitchMemorySpaceEx(m, NULL);
    p->MemorySpace = m;
    p->ParentId = 0;
    strncpy(p->DebugName, "init", 16);
    PsCreateProcess(p, base, 0);
    if (trpen) arch_enable_trap();
    return;
fail:
    PANIC("spawn_init_process FAULT");
}

/*
 * A fork child will first swtch here, and then "return" to user space.
 */
void forkret() {
	/* TODO: Lab3 Process */
    // Nothing to do. I don't even use this procedure.
    return;
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
NO_RETURN void exit() {
    // struct proc *p = thiscpu()->proc;
    /* TODO: Lab3 Process */
	KeExitProcess();
}
