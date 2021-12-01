/*
 * Context switch
 *
 *   void swtch(struct context **old, struct context *new);
 * 
 * Save the current registers on the stack, creating
 * a struct context, and save its address in *old.
 * Switch stacks to new and pop previously-saved registers.
 *
 * Function `swtch` is call-return semantic.
 * However, the pc+4 of the caller is saved in the context.
 * Therefore, we can store pc+4 in a caller-saved register (like x15),
 * and use `br x15` instead of `ret` in the return stage.
 */
.global swtch
swtch:
    /* TODO: Lab3 Schedule */
    // (Uncomment on demand)
    /*
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    */
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
    mov x2, sp
    str x2, [x1] // oldkstack
    mov sp, x0 // kstack
    ldp x0, x1, [sp], #16
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    /*
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    */
    ret
