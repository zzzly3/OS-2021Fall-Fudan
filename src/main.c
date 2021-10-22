#if 0
#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/virtual_memory.h>
#include <core/sched.h>
#include <core/trap.h>
#include <driver/clock.h>
#include <driver/interrupt.h>
#include <core/proc.h>

struct cpu cpus[NCPU];

void hello() {
    printf("CPU %d: HELLO!\n", cpuid());
    reset_clock(1000);
}

void init_system_per_cpu() {
    init_clock();
    set_clock_handler(hello);
    init_trap();

    init_cpu(&simple_scheduler);
}

NORETURN void main() {
    // initialize BSS sections.
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    init_interrupt();
    init_char_device();
    init_console();
	/* TODO: Lab1 print */

    init_memory_manager();
    //init_virtual_memory();

    init_system_per_cpu();

	/* TODO: Lab3 uncomment to test interrupt */
    // test_kernel_interrupt();
    if (cpuid() == 0) {
        spawn_init_process();
        enter_scheduler();
    } else {
        enter_scheduler();
    }

}
#else
#include <def.h>
#include <core/console.h>

void sys_test_callback()
{
    while(1)
    {
        printf("CPU %d hello!\n", cpuid());
        delay_us(1000 * 1000);
    }
}

NO_RETURN main()
{
    system_init();
    printf("CPU %d init.\n", cpuid());
    // asserts(1==2, "PASS");
    int pid;
    if (KSUCCESS(KeCreateProcess(NULL, (PVOID)sys_test_callback, 0, &pid)))
    {
        KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
        while(1);
    }
    else
        KeBugFault(0x19260817);
}

#endif
