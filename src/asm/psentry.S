// The user process entry. Use assembly to avoid building trapframe.
.global PsUserProcessEntry
PsUserProcessEntry: // x0: UserEntry; x1: UserArgument
	mov x19, x0
	mov x20, x1
	bl PsiProcessEntry
	msr elr_el1, x19
	mov x0, #0
	msr spsr_el1, x0
	mov x0, x20
	eret

.global PsKernelProcessEntry
PsKernelProcessEntry: // x0: KernelEntry; x1: KernelArgument
	mov x19, x0
	mov x20, x1
	bl PsiProcessEntry
	mov x0, #0
	msr daif, x0
	mov x0, x20
	mov x30, =KeExitProcess // return to exit
	br x19
	