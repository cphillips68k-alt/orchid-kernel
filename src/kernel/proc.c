#include "proc.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"
#include "sync.h"
#include "scheduler.h"
#include <stddef.h>

extern uint64_t kernel_cr3;
extern uint64_t hhdm_offset;
extern uint64_t saved_user_rip;

#define USER_STACK_VADDR 0x800000000000

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
     * The stack pointer is the same virtual address as the parent's.
     */
    thread_t *child_thread = thread_create_user(
        saved_user_rip,
        USER_STACK_VADDR + PAGE_SIZE,
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

    child->next = process_list;
    process_list = child;

    spin_unlock(&proc_lock);
    return child->pid;
}