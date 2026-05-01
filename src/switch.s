.section .text
.global __switch_to
.type __switch_to, @function

__switch_to:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq (%rsi), %rsp

    /* Switch page tables if needed */
    movq %rdx, %rax
    movq %cr3, %rcx
    cmpq %rax, %rcx
    je 1f
    movq %rax, %cr3
1:

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret