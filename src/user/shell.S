.section .text
.globl _start
_start:
    # write(1, prompt, 2)
    mov    $1, %rax
    mov    $1, %rdi
    lea    prompt(%rip), %rsi
    mov    $2, %rdx
    syscall

    # read(0, buf, 128)
    mov    $0, %rax
    mov    $0, %rdi
    lea    buf(%rip), %rsi
    mov    $128, %rdx
    syscall

    test   %rax, %rax
    jle    _start

    mov    %rax, %r12

    # write(1, buf, r12)
    mov    $1, %rax
    mov    $1, %rdi
    lea    buf(%rip), %rsi
    mov    %r12, %rdx
    syscall

    jmp    _start

.section .rodata
prompt:
    .asciz "$ "

.section .bss
buf:
    .space 128