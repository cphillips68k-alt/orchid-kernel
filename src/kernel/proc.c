#include "proc.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"
#include "sync.h"
#include "scheduler.h"
#include <stddef.h>

#define USER_STACK_VADDR 0x800000000000

extern uint64_t kernel_cr3;
extern uint64_t hhdm_offset;
extern uint64_t saved_user_rip;  /* set by syscall_entry.S */

static process_t *process_list = NULL;
process_t *current_process = NULL;
static uint64_t next_pid = 1;
static spinlock_t proc_lock = 0;

static void *memcpy(void *d, const void *s, size_t n) {
    char *dst = d; const char *src = s;
    for (size_t i = 0; i < n; i++) dst[i] = src[i];
    return d;
}

static void memset(void *s, int c, size_t n) {
    char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (char)c;
}

void proc_init(void) { }
void proc_set_current(process_t *p) { current_process = p; }

uint64_t proc_alloc_pid(void) {
    spin_lock(&proc_lock);
    uint64_t pid = next_pid++;
    spin_unlock(&proc_lock);
    return pid;
}

/*
 * Deep copy the user address space.
 * Allocates independent page tables for the child.
 * Returns 0 on success, -1 on out-of-memory.
 */
static int fork_address_space(uint64_t parent_pml4_phys, uint64_t child_pml4_phys) {
    uint64_t *parent_pml4 = (uint64_t *)(parent_pml4_phys + hhdm_offset);
    uint64_t *child_pml4  = (uint64_t *)(child_pml4_phys + hhdm_offset);

    /* Copy kernel higher-half (shared) */
    for (int i = 256; i < 512; i++) {
        child_pml4[i] = parent_pml4[i];
    }

    /* Deep copy user half */
    for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
        if (!(parent_pml4[pml4_i] & 1))
            continue;

        uint64_t child_pdpt_phys = pmm_alloc_page();
        if (!child_pdpt_phys) return -1;
        uint64_t *child_pdpt = (uint64_t *)(child_pdpt_phys + hhdm_offset);
        memset(child_pdpt, 0, PAGE_SIZE);
        child_pml4[pml4_i] = (parent_pml4[pml4_i] & 0xFFF) | child_pdpt_phys;

        uint64_t *parent_pdpt = (uint64_t *)((parent_pml4[pml4_i] & 0xFFFFFFFFFF000ULL) + hhdm_offset);

        for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            if (!(parent_pdpt[pdpt_i] & 1))
                continue;

            uint64_t child_pd_phys = pmm_alloc_page();
            if (!child_pd_phys) return -1;
            uint64_t *child_pd = (uint64_t *)(child_pd_phys + hhdm_offset);
            memset(child_pd, 0, PAGE_SIZE);
            child_pdpt[pdpt_i] = (parent_pdpt[pdpt_i] & 0xFFF) | child_pd_phys;

            uint64_t *parent_pd = (uint64_t *)((parent_pdpt[pdpt_i] & 0xFFFFFFFFFF000ULL) + hhdm_offset);

            for (int pd_i = 0; pd_i < 512; pd_i++) {
                if (!(parent_pd[pd_i] & 1))
                    continue;

                uint64_t child_pt_phys = pmm_alloc_page();
                if (!child_pt_phys) return -1;
                uint64_t *child_pt = (uint64_t *)(child_pt_phys + hhdm_offset);
                memset(child_pt, 0, PAGE_SIZE);
                child_pd[pd_i] = (parent_pd[pd_i] & 0xFFF) | child_pt_phys;

                uint64_t *parent_pt = (uint64_t *)((parent_pd[pd_i] & 0xFFFFFFFFFF000ULL) + hhdm_offset);

                for (int pt_i = 0; pt_i < 512; pt_i++) {
                    if (!(parent_pt[pt_i] & 1))
                        continue;

                    uint64_t old_phys = parent_pt[pt_i] & 0xFFFFFFFFFF000ULL;
                    uint64_t new_phys = pmm_alloc_page();
                    if (!new_phys) return -1;

                    void *src  = (void *)(old_phys + hhdm_offset);
                    void *dst  = (void *)(new_phys + hhdm_offset);
                    memcpy(dst, src, PAGE_SIZE);

                    child_pt[pt_i] = (parent_pt[pt_i] & 0xFFF) | new_phys;
                }
            }
        }
    }

    return 0;
}

uint64_t sys_fork(void) {
    spin_lock(&proc_lock);

    if (!current_process) {
        spin_unlock(&proc_lock);
        return (uint64_t)-1;
    }

    process_t *child = kmalloc(sizeof(process_t));
    if (!child) {
        spin_unlock(&proc_lock);
        return (uint64_t)-1;
    }

    child->pid = next_pid++;
    child->next = NULL;

    uint64_t child_pml4_phys = pmm_alloc_page();
    if (!child_pml4_phys) {
        kfree(child);
        spin_unlock(&proc_lock);
        return (uint64_t)-1;
    }

    if (fork_address_space(current_process->pml4_phys, child_pml4_phys) != 0) {
        pmm_free_page(child_pml4_phys);
        kfree(child);
        spin_unlock(&proc_lock);
        return (uint64_t)-1;
    }

    child->pml4_phys = child_pml4_phys;

    /*
     * Create the child thread.
     * The child resumes at the same user-mode instruction as the parent
     * (the instruction after the 'syscall' that invoked fork).
     * 
     * User stack: same virtual address as parent (each has independent 
     * physical pages thanks to fork_address_space).
     *
     * We pass USER_STACK_VADDR + PAGE_SIZE as the initial stack pointer,
     * same as load_raw does. The stack pages were already deep-copied,
     * so the child has an identical stack layout.
     */
    thread_t *child_thread = thread_create_user(
        saved_user_rip,                    /* same user RIP as parent */
        USER_STACK_VADDR + PAGE_SIZE,      /* same stack top */
        child_pml4_phys,
        child
    );

    if (!child_thread) {
        pmm_free_page(child_pml4_phys);
        kfree(child);
        spin_unlock(&proc_lock);
        return (uint64_t)-1;
    }

    child->main_thread = child_thread;

    /* 
     * Set the child's return value to 0.
     * thread_create_user builds a stack frame for userspace_entry + iretq.
     * The RAX value that the user sees after sysret comes from the RAX 
     * field in that frame. We need to poke it.
     *
     * The frame built by thread_create_user has the iretq frame at the top,
     * then a return address pointing to userspace_entry.
     * Actually, looking at thread_create_user:
     *
     *   stk -= 5;
     *   stk[0] = 0x23;        // SS
     *   stk[1] = user_stack;  // RSP
     *   stk[2] = 0x202;       // RFLAGS
     *   stk[3] = 0x1B;        // CS
     *   stk[4] = user_entry;  // RIP
     *   stk--;                // push return address to userspace_entry
     *   *stk = userspace_entry;
     *   stk -= 6;             // push 6 zero words
     *
     * So the iret frame has RIP at stk[4] (after the stack pointer adjustments).
     * We need to set the return-value register (RAX) in the child's initial
     * register state. Looking at the layout more carefully:
     *
     * t->rsp points to the "6 zero words", then above that is the address of
     * userspace_entry. When userspace_entry does 'iretq', it uses the 5-word
     * frame starting above the return address.
     *
     * After iretq, the user sees the registers as saved by the hardware.
     * But RAX isn't set by iretq — it retains whatever value was in RAX
     * when iretq executes. userspace_entry doesn't set RAX explicitly.
     *
     * The child needs to see RAX=0 when it starts executing. The cleanest
     * way is to modify the thread's initial register state, but the current 
     * thread_create_user doesn't set RAX.
     *
     * We need to add RAX to the thread context, or handle this differently.
     * For now, we'll modify the child's starting frame to have RAX=0.
     *
     * Looking at the 6 zero words pushed by thread_create_user, those are 
     * likely for callee-saved registers. We need to find which one is RAX.
     * 
     * Actually, userspace_entry.S just does 'iretq'. RAX will be whatever
     * the C code left it as before returning. In syscall_entry, after 
     * calling syscall_handler, we do:
     *   movq syscall_retval(%rip), %rax
     *   popq %rcx
     *   popq %r11
     *   sysretq
     *
     * For the parent, RAX is set to syscall_retval (= child's PID).
     * For the child, we need RAX to be 0.
     *
     * The simplest fix: modify thread_create_user to accept a return value,
     * or set it after creation. Let's add a field to thread_t.
     */

    /*
     * For now, the child will execute from saved_user_rip with the correct
     * stack. It sees RAX = whatever syscall_retval was set to before fork().
     * We need to set syscall_retval = 0 for the child.
     *
     * Since both parent and child share the same syscall_retval global,
     * we have a problem: we can't set different return values for each.
     *
     * The clean solution: don't use a global for the return value.
     * Instead, store it in the thread structure and have syscall_entry
     * load it from current_thread.
     *
     * Quick fix for now: add a 'syscall_retval' field to thread_t,
     * set it in syscall_handler, and have syscall_entry load from
     * current_thread->syscall_retval instead of the global.
     */

    spin_unlock(&proc_lock);
    return child->pid;
}