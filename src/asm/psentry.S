// The user process entry. Use assembly to avoid building trapframe.
.global PsUserProcessEntry
PsUserProcessEntry: // x0: UserEntry; x1: UserArgument
	msr elr_el1, x0
	mov x0, #0
	msr spsr_el1, x0
	mov x0, x1
	eret

.global PsKernelProcessEntry
PsKernelProcessEntry:
	mov x2, #0
	msr daif, x2
	mov x2, x0
	mov x0, x1
	br x2