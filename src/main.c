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

void sys_switch_test();
void sys_transfer_test();
void spawn_init_process();

NO_RETURN main()
{
    system_init();
    printf("CPU %d init.\n", cpuid());
    spawn_init_process();
    if (cpuid() == 0)
        KeCreateDpc(sys_switch_test, 0);
        // KeCreateDpc(sys_transfer_test, 0);
    KeSystemWorkerEntry();
}

#endif
