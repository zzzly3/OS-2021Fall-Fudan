#include <stdint.h>

#include <core/proc.h>
#include <core/sched.h>
#include <core/syscall.h>
#include <core/trap.h>

int sys_yield() {
    yield();
    return 0;
}

/* 
 * Get the parameters and call growproc.
 */
usize sys_brk(usize n) {
    /* TODO: Lab9 Shell */
    if (n >= 0x7fffffff)
        return -1;
    return growproc(n);
}

int sys_clone(PTRAP_FRAME tf) {
    if (tf->x0 != 17) {
        printf("sys_clone: flags other than SIGCHLD are not supported.\n");
        return -1;
    }
    return fork(tf);
}

int sys_wait4(PTRAP_FRAME tf) {
    i32 pid = tf->x0, opt = tf->x1;
    int *wstatus = (int*)tf->x2;
    void *rusage = (void*)tf->x3;
    // if (argint(0, &pid) < 0 || argint(1, &wstatus) < 0 || argint(2, &opt) < 0 ||
    //     argint(3, &rusage) < 0)
    //     return -1;

    if (pid != -1 || wstatus != 0 || opt != 0 || rusage != 0) {
        printf("sys_wait4: unimplemented. pid %d, wstatus 0x%p, opt 0x%x, rusage 0x%p\n",
               pid,
               wstatus,
               opt,
               rusage);
        while (1) {}
        return -1;
    }

    return wait();
}

int sys_exit() {
    exit();
}