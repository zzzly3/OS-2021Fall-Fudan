#include <core/console.h>
#include <core/cpu.h>
#include <core/proc.h>
#include <core/syscall.h>
#include <ob/proc.h>

void sys_myexecve(char *s) {
    printf("sys_exec: executing %s\n", s);
    return;
}

NO_RETURN void sys_myexit() {
    printf("sys_exit: in exit\n");
    exit();
}

/* myprint(int x) =>
            printf("pid %d, pid in root %d, cnt %d\n", getpid(), getrootpid(), x);
            yield(); */
void sys_myprint(int x) {
    printf("pid %d, pid in root %d, cnt %d\n", PsGetCurrentProcessId(), PsGetCurrentProcess()->ProcessId, x);
    arch_disable_trap();
    KeTaskSwitch();
    arch_enable_trap();
}
