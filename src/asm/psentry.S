// The process entry. Use assembly to avoid building trapframe.
.global PsUserProcessEntry
PsUserProcessEntry: // x0: UserEntry; x1: UserArgument
	msr elr_el1, x0
	mov x0, #0
	msr spsr_el1, x0
	mov x0, x1
	eret