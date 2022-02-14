#include <aarch64/intrinsic.h>
#include <core/console.h>
#include <core/syscall.h>
#include <core/trap.h>
#include <driver/interrupt.h>
#include <driver/uart.h>
#include <driver/clock.h>
#include <driver/irq.h>
#include <mod/scheduler.h>
#include <mod/syscall.h>
#include <mod/bug.h>

void KiExceptionEntry(PTRAP_FRAME, ULONG64);

void init_trap() {
    extern char exception_vector[];
    arch_set_vbar(exception_vector);
    arch_reset_esr();
}

void trap_global_handler(Trapframe *frame) {
    u64 esr = arch_get_esr();
    u64 ec = esr >> ESR_EC_SHIFT;
    u64 iss = esr & ESR_ISS_MASK;
    u64 ir = esr & ESR_IR_MASK;
    // uart_put_char('t');

// u32 src = get32(IRQ_SRC_CORE(cpuid()));

    arch_reset_esr();

    switch (ec) {
        case ESR_EC_UNKNOWN: {
            if (ir)
                PANIC("unknown error");
            else
                interrupt_global_handler();
        } break;

        case ESR_EC_SVC64: {
            /*
			 * Here, it is a syscall request.
			 * Call `syscall_dispatch` and
			 * record the return value.
			 * Note: this function returns void,
			 * where to record the return value?
			 */
			/* TODO: Lab3 Syscall */
            KiSystemCallEntry((PTRAP_FRAME)frame);
            // TODO: warn if `iss` is not zero.
            (void)iss;
        } break;

        default: {
            // TODO: should exit current process here.
            // exit(1);
            KiExceptionEntry((PTRAP_FRAME)frame, esr);
        }
    }
    /*
	 * Hint: For testing, you can set frame->x6 to 0xdead here.
	 * Use GDB to check whether x6 is correct after `trap_return` finishes.
	 * If another register changes to 0xdead, fix the bug in trapframe design.
	 */
}

NORETURN void trap_error_handler(u64 type) {
    PANIC("unknown trap type: %d", type);
}
