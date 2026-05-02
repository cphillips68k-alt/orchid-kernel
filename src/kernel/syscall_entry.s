.section .text
.global syscall_entry
.type syscall_entry, @function
.extern syscall_handler
.extern syscall_retval

syscall_entry:
    pushq %rcx
    pushq %r11

    movq %r10, %rcx
    call syscall_handler

    movq syscall_retval(%rip), %rax

    popq %r11
    popq %rcx
    sysretq