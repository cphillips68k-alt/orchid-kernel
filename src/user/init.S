.section .text
.globl _start
_start:
    # sys_get_binary("shell", buf, 4096)
    lea    shell_name(%rip), %rdi
    lea    shell_buf(%rip), %rsi
    mov    $4096, %rdx
    mov    $11, %rax
    syscall

    mov    %rax, %r12
    test   %rax, %rax
    jz     loop_forever
    cmp    $4096, %rax
    ja     loop_forever

    # fork
    mov    $7, %rax
    syscall

    test   %rax, %rax
    jnz    loop_forever

    # child: exec(shell_buf, size)
    lea    shell_buf(%rip), %rdi
    mov    %r12, %rsi
    mov    $10, %rax
    syscall

    # fallback exit
    mov    $60, %rax
    xor    %rdi, %rdi
    syscall

loop_forever:
    mov    $158, %rax
    syscall
    jmp    loop_forever

.section .rodata
shell_name:
    .asciz "shell"

.section .bss
shell_buf:
    .space 4096