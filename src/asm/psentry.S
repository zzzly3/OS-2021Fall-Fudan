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
	ldr x30, =KeExitProcess // return to exit
	br x19
	
.global PsUserForkEntry
PsUserForkEntry: // x0: context address
	mov x27, x0
	bl PsiProcessEntry
	ldp x0, x1, [x27], #16
    msr elr_el1, x0
    msr spsr_el1, x1
    ldp x8, x9, [x27], #16
    ldp x0, x1, [x27], #16
    ldp x29, x30, [x27], #16
    ldp x16, x17, [x27], #16
    ldp x14, x15, [x27], #16
    ldp x12, x13, [x27], #16
    ldp x10, x11, [x27], #16
    ldp x6, x7, [x27], #16
    ldp x4, x5, [x27], #16
    ldp x2, x3, [x27], #16
    ldp x19, x20, [x27], #16
    ldp x21, x22, [x27], #16
    ldp x23, x24, [x27], #16
    ldp x25, x26, [x27], #16
    ldp x27, x28, [x27]
    eret
