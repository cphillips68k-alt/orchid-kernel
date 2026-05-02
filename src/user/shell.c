/* shell.c – simple user-space shell, echoes input */
void _start(void) {
    char buf[128];
    const char prompt[] = "$ ";

    while (1) {
        /* write prompt */
        __asm__ volatile (
            "mov $1, %%rax\n"   /* syscall write */
            "mov $1, %%rdi\n"   /* fd = stdout */
            "mov %0, %%rsi\n"   /* buf */
            "mov %1, %%rdx\n"   /* len */
            "syscall\n"
            :
            : "r"(prompt), "r"((unsigned long)2)
            : "rax","rdi","rsi","rdx"
        );

        /* read line */
        int n = 0;
        while (n < 127) {
            char c;
            __asm__ volatile (
                "mov $0, %%rax\n"
                "mov $0, %%rdi\n"
                "mov %0, %%rsi\n"
                "mov $1, %%rdx\n"
                "syscall\n"
                : "=a"(c)
                : "r"(&c)
                : "rdi","rsi","rdx"
            );
            if (c == '\n' || c == '\r') break;
            buf[n++] = c;
        }
        buf[n] = '\0';

        /* echo back */
        if (n > 0) {
            buf[n] = '\n';
            __asm__ volatile (
                "mov $1, %%rax\n"
                "mov $1, %%rdi\n"
                "mov %0, %%rsi\n"
                "mov %1, %%rdx\n"
                "syscall\n"
                :
                : "r"(buf), "r"((unsigned long)(n+1))
                : "rax","rdi","rsi","rdx"
            );
        }
    }
}