.section .text
.global syscall_entry
.type syscall_entry, @function
.extern syscall_handler
.extern syscall_retval
.extern saved_user_rip

syscall_entry:
    /* Save user's RCX (return RIP) and R11 (RFLAGS) for SYSRET */
    pushq %r11
    pushq %rcx

    /* Save user RIP before we clobber RCX */
    movq %rcx, saved_user_rip(%rip)

    /* Rearrange registers for C calling convention.
     *
     * SYSCALL gives us:  RAX=nr, RDI=a1, RSI=a2, RDX=a3, R10=a4, R8=a5, R9=a6
     * C handler expects: RDI=nr, RSI=a1, RDX=a2, RCX=a3, R8=a4,  R9=a5
     */
    movq %rdx, %rcx              /* a3 -> RCX */
    movq %rsi, %rdx              /* a2 -> RDX */
    movq %rdi, %rsi              /* a1 -> RSI */
    movq %rax, %rdi              /* nr -> RDI */
    movq %r10, %r8               /* a4 -> R8  */
    /* R9 already holds a5 from the user's R8 */

    call syscall_handler

    movq syscall_retval(%rip), %rax

    popq %rcx                    /* restore user RIP */
    popq %r11                    /* restore user RFLAGS */
    sysretq