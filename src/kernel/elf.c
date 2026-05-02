#include "elf.h"
#include "pmm.h"
#include "vmm.h"
#include "proc.h"
#include "scheduler.h"
#include "serial.h"
#include <stddef.h>

extern uint64_t hhdm_offset;
extern uint64_t kernel_cr3;

#define USER_CODE_VADDR  0x400000
#define USER_STACK_VADDR 0x800000000000

static void *memcpy(void *d, const void *s, size_t n) {
    char *dst = d; const char *src = s;
    for (size_t i = 0; i < n; i++) dst[i] = src[i];
    return d;
}

static void memset(void *s, int c, size_t n) {
    char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = c;
}

static thread_t *load_raw(const uint8_t *data, size_t size, process_t *proc) {
    serial_printf("[ELF] Loading raw binary: size=%d\n", size);

    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) return NULL;

    uint64_t *pml4 = (uint64_t *)(pml4_phys + hhdm_offset);
    memset(pml4, 0, PAGE_SIZE);

    uint64_t *old_pml4 = (uint64_t *)(kernel_cr3 + hhdm_offset);
    for (int i = 256; i < 512; i++) pml4[i] = old_pml4[i];

    uint64_t vaddr = USER_CODE_VADDR;
    const uint8_t *src = data;
    size_t left = size;
    while (left) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return NULL;
        vmm_map_user(pml4_phys, vaddr, phys,
                     VMM_PRESENT | VMM_USER | VMM_WRITABLE, 1);

        size_t chunk = left > PAGE_SIZE ? PAGE_SIZE : left;
        memcpy((void *)(phys + hhdm_offset), src, chunk);
        if (chunk < PAGE_SIZE)
            memset((void *)(phys + hhdm_offset + chunk), 0, PAGE_SIZE - chunk);

        vaddr += PAGE_SIZE;
        src   += chunk;
        left  -= chunk;
    }

    uint64_t stack_phys = pmm_alloc_page();
    if (!stack_phys) return NULL;
    vmm_map_user(pml4_phys, USER_STACK_VADDR, stack_phys,
                 VMM_PRESENT | VMM_USER | VMM_WRITABLE, 1);

    proc->pml4_phys = pml4_phys;

    thread_t *t = thread_create_user(USER_CODE_VADDR,
                                     USER_STACK_VADDR + PAGE_SIZE,
                                     pml4_phys, proc);
    return t;
}

int elf_load(const uint8_t *data, size_t size) {
    process_t *proc = kmalloc(sizeof(process_t));
    if (!proc) return -1;

    proc->pid = proc_alloc_pid();
    proc->next = NULL;
    /* current_process will be set by the scheduler */

    thread_t *t = load_raw(data, size, proc);
    if (!t) {
        kfree(proc);
        return -1;
    }
    proc->main_thread = t;
    scheduler_add_thread(t);

    return (int)proc->pid;
}

void sys_exec(const uint8_t *data, size_t size) {
    if (!current_process) return;
    thread_t *t = load_raw(data, size, current_process);
    if (!t) return;
    current_process->main_thread = t;
    scheduler_add_thread(t);
    thread_exit();
}