/* init.c – first user process, launches the shell */
void _start(void) {
    const char name[] = "shell";
    /* then use name in the syscall */
    
    /* Get the shell binary */
    char shell_buf[4096];  /* hopefully the shell is small */
    unsigned long size = 0;
    __asm__ volatile (
        "mov $11, %%rax\n"       /* sys_get_binary */
        "mov $name, %%rdi\n"     /* "shell" */
        "mov %1, %%rsi\n"        /* buffer */
        "mov $4096, %%rdx\n"     /* max size */
        "syscall\n"
        "mov %%rax, %0\n"
        : "=r"(size)
        : "r"(shell_buf)
        : "rax","rdi","rsi","rdx"
    );
    if (size == 0 || size > 4096) {
        /* fallback: just loop */
        while (1) __asm__ volatile ("mov $158, %%rax; syscall");
    }

    /* Fork */
    unsigned long pid;
    __asm__ volatile (
        "mov $7, %%rax\n"
        "syscall\n"
        "mov %%rax, %0\n"
        : "=r"(pid)
        :
        : "rax"
    );

    if (pid == 0) {
        /* Child: exec the shell */
        __asm__ volatile (
            "mov $10, %%rax\n"   /* sys_exec */
            "mov %0, %%rdi\n"
            "mov %1, %%rsi\n"
            "syscall\n"
            :
            : "r"(shell_buf), "r"(size)
            : "rax","rdi","rsi"
        );
        /* If exec fails, just exit */
        __asm__ volatile ("mov $60, %%rax; xor %%rdi, %%rdi; syscall");
    } else {
        /* Parent: wait for child? For now, just loop. */
        while (1) __asm__ volatile ("mov $158, %%rax; syscall");
    }
}