.section .text
.globl _start
_start:
    # write(1, prompt, 2)
    mov    $1, %rax
    mov    $1, %rdi
    lea    prompt(%rip), %rsi
    mov    $2, %rdx
    syscall

    # read(0, buf, 128) – block until input
    mov    $0, %rax
    mov    $0, %rdi
    lea    buf(%rip), %rsi
    mov    $128, %rdx
    syscall

    # if read <= 0, repeat prompt
    test   %rax, %rax
    jle    _start

    # replace last char with newline (for echo)
    # find actual length? we'll echo the whole input line
    # we just print the buffer back
    mov    %rax, %r12          # save byte count

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