.section .text
.globl _start
_start:
    # sys_get_binary("shell", buf, 4096)
    lea    shell_name(%rip), %rdi
    lea    shell_buf(%rip), %rsi
    mov    $4096, %rdx
    mov    $11, %rax           # SYS_get_binary
    syscall

    # save size in %r12
    mov    %rax, %r12
    test   %rax, %rax
    jz     loop_forever
    cmp    $4096, %rax
    ja     loop_forever

    # sys_fork()
    mov    $7, %rax
    syscall

    # parent loop if rax != 0, else exec
    test   %rax, %rax
    jnz    loop_forever

    # child: sys_exec(shell_buf, size)
    lea    shell_buf(%rip), %rdi
    mov    %r12, %rsi
    mov    $10, %rax           # SYS_exec
    syscall

    # fallback exit if exec failed
    mov    $60, %rax
    xor    %rdi, %rdi
    syscall

loop_forever:
    mov    $158, %rax          # SYS_yield
    syscall
    jmp    loop_forever

.section .rodata
shell_name:
    .asciz "shell"

.section .bss
shell_buf:
    .space 4096