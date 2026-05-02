#include "user.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"
#include "scheduler.h"
#include "sync.h"
#include <stddef.h>

#define USER_CODE_VADDR 0x400000
#define USER_STACK_VADDR 0x8000000000
#define USER_STACK_SIZE  PAGE_SIZE
#define STACK_SIZE       4096      /* kernel stack size for the user thread's TCB */

extern uint64_t hhdm_offset;
extern uint64_t kernel_cr3;

__asm__ (
".section .rodata\n"
".global user_code_start\n"
".global user_code_end\n"
"user_code_start:\n"
    "mov $1, %rax\n"          /* syscall: write */
    "mov $1, %rdi\n"          /* fd = stdout */
    "lea msg(%rip), %rsi\n"
    "mov $20, %rdx\n"
    "syscall\n"
    "mov $60, %rax\n"         /* syscall: exit */
    "xor %rdi, %rdi\n"
    "syscall\n"
    "jmp .\n"
"msg:\n"
    ".ascii \"Hello from user! :^)\\n\"\n"
"user_code_end:\n"
);

extern char user_code_start[];
extern char user_code_end[];

static void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

static void memset(void *s, int c, size_t n) {
    char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = c;
}

thread_t *user_thread_create(void) {
    /* 1. Allocate new PML4 for user address space */
    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) return NULL;
    uint64_t *pml4 = (uint64_t *)(pml4_phys + hhdm_offset);
    memset(pml4, 0, PAGE_SIZE);

    /* 2. Copy kernel-space mappings (top 256 entries) */
    uint64_t *old_pml4 = (uint64_t *)(kernel_cr3 + hhdm_offset);
    for (int i = 256; i < 512; i++) {
        pml4[i] = old_pml4[i];
    }

    /* 3. Map user code page */
    uint64_t code_phys = pmm_alloc_page();
    if (!code_phys) return NULL;
    void *code_virt = (void *)(code_phys + hhdm_offset);
    size_t code_sz = (size_t)(user_code_end - user_code_start);
    if (code_sz > PAGE_SIZE) code_sz = PAGE_SIZE;
    memcpy(code_virt, user_code_start, code_sz);
    vmm_map_user(pml4_phys, USER_CODE_VADDR, code_phys,
                 VMM_PRESENT | VMM_USER, 0);

    /* 4. Map user stack page */
    uint64_t stack_phys = pmm_alloc_page();
    if (!stack_phys) return NULL;
    vmm_map_user(pml4_phys, USER_STACK_VADDR, stack_phys,
                 VMM_PRESENT | VMM_USER | VMM_WRITABLE, 0);
    uint64_t user_stack_top = USER_STACK_VADDR + PAGE_SIZE;

    /* 5. Create kernel thread that will iret to ring 3 */
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) return NULL;
    void *kstack = kmalloc(STACK_SIZE);   /* <--- now STACK_SIZE is defined */
    if (!kstack) { kfree(t); return NULL; }
    t->kernel_stack = (uint64_t)kstack + STACK_SIZE;
    t->cr3 = pml4_phys;

    /* Build iret frame */
    uint64_t *stk = (uint64_t *)t->kernel_stack - 5;  /* iret frame */
    stk[0] = 0x23;                /* SS = user data selector */
    stk[1] = user_stack_top;     /* RSP */
    stk[2] = 0x202;              /* RFLAGS */
    stk[3] = 0x1B;               /* CS = user code selector */
    stk[4] = USER_CODE_VADDR;    /* RIP */
    stk -= 2;                     /* int_no, err_code */
    stk[0] = 0; stk[1] = 0;
    stk -= 15;                    /* zeroed registers */
    for (int i = 0; i < 15; i++) stk[i] = 0;
    t->rsp = (uint64_t)stk;

    t->state = THREAD_STATE_READY;
    scheduler_add_thread(t);
    return t;
}