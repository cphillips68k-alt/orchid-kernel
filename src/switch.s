.section .text
.global __switch_to
.type __switch_to, @function

// void __switch_to(thread_t *old, thread_t *new)
// old->rsp = rsp, then rsp = new->rsp, then ret
__switch_to:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)      // old->rsp = rsp
    movq (%rsi), %rsp      // rsp = new->rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret