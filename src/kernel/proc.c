#include "proc.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"
#include "sync.h"
#include "scheduler.h"
#include <stddef.h>

extern uint64_t kernel_cr3;
extern uint64_t hhdm_offset;

static process_t *process_list = NULL;
process_t *current_process = NULL;
static uint64_t next_pid = 1;
static spinlock_t proc_lock = 0;

static void *memcpy(void *d, const void *s, size_t n) {
    char *dst = d; const char *src = s;
    for (size_t i = 0; i < n; i++) dst[i] = src[i];
    return d;
}

void proc_init(void) { }

void proc_set_current(process_t *p) { current_process = p; }

uint64_t proc_alloc_pid(void) {
    spin_lock(&proc_lock);
    uint64_t pid = next_pid++;
    spin_unlock(&proc_lock);
    return pid;
}

uint64_t sys_fork(void) {
    spin_lock(&proc_lock);

    process_t *child = kmalloc(sizeof(process_t));
    if (!child) { spin_unlock(&proc_lock); return (uint64_t)-1; }
    child->pid = next_pid++;
    child->next = NULL;

    uint64_t new_pml4_phys = pmm_alloc_page();
    if (!new_pml4_phys) { kfree(child); spin_unlock(&proc_lock); return (uint64_t)-1; }
    uint64_t *new_pml4 = (uint64_t*)(new_pml4_phys + hhdm_offset);
    uint64_t *old_pml4 = (uint64_t*)(current_process->pml4_phys + hhdm_offset);
    for (int i = 0; i < 512; i++) new_pml4[i] = old_pml4[i];
    child->pml4_phys = new_pml4_phys;

    /* Deep copy user pages */
    for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
        if (!(old_pml4[pml4_i] & 1)) continue;
        uint64_t *old_pdpt = (uint64_t*)((old_pml4[pml4_i] & 0xFFFFFFFFFF000) + hhdm_offset);
        uint64_t *new_pdpt = (uint64_t*)((new_pml4[pml4_i] & 0xFFFFFFFFFF000) + hhdm_offset);
        for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            if (!(old_pdpt[pdpt_i] & 1)) continue;
            uint64_t *old_pd = (uint64_t*)((old_pdpt[pdpt_i] & 0xFFFFFFFFFF000) + hhdm_offset);
            uint64_t *new_pd = (uint64_t*)((new_pdpt[pdpt_i] & 0xFFFFFFFFFF000) + hhdm_offset);
            for (int pd_i = 0; pd_i < 512; pd_i++) {
                if (!(old_pd[pd_i] & 1)) continue;
                uint64_t *old_pt = (uint64_t*)((old_pd[pd_i] & 0xFFFFFFFFFF000) + hhdm_offset);
                uint64_t *new_pt = (uint64_t*)((new_pd[pd_i] & 0xFFFFFFFFFF000) + hhdm_offset);
                for (int pt_i = 0; pt_i < 512; pt_i++) {
                    if (!(old_pt[pt_i] & 1)) continue;
                    uint64_t old_phys = old_pt[pt_i] & 0xFFFFFFFFFF000;
                    uint64_t new_phys = pmm_alloc_page();
                    if (!new_phys) { spin_unlock(&proc_lock); return (uint64_t)-1; }
                    vmm_map(0xFFFFFF0000000000, new_phys, VMM_PRESENT | VMM_WRITABLE);
                    memcpy((void*)0xFFFFFF0000000000,
                           (void*)(old_phys + hhdm_offset), PAGE_SIZE);
                    vmm_unmap(0xFFFFFF0000000000);
                    new_pt[pt_i] = (old_pt[pt_i] & 0xFFF) | new_phys;
                }
            }
        }
    }

    thread_t *parent_thread = current_process->main_thread;
    uint64_t parent_rip = ((uint64_t*)parent_thread->rsp)[17];
    thread_t *child_thread = thread_create(NULL, "child", new_pml4_phys, child);
    if (!child_thread) { spin_unlock(&proc_lock); return (uint64_t)-1; }
    uint64_t *cframe = (uint64_t*)child_thread->rsp;
    cframe[17] = parent_rip;
    cframe[7]  = 0;   /* fork returns 0 to child */
    child->main_thread = child_thread;
    child->next = process_list;
    process_list = child;

    spin_unlock(&proc_lock);
    return child->pid;
}