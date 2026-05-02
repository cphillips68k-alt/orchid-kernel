/* init.c – first user process, loaded by the kernel's ELF loader.
   For now, it just loops yielding. Once we have fork/exec fully working,
   init will spawn the VFS, keyboard driver, and shell. */

void _start(void) {
    while (1) {
        /* sys_yield() */
        __asm__ volatile ("mov $158, %%rax; syscall" ::: "rax");
    }
}