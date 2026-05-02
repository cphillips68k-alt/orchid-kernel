#include "elf.h"
#include "pmm.h"
#include "vmm.h"
#include "proc.h"
#include "scheduler.h"
#include "serial.h"
#include <stddef.h>

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD 1
#define PF_R 4
#define PF_W 2
#define PF_X 1

extern uint64_t hhdm_offset;
extern uint64_t kernel_cr3;
extern process_t *current_process;

static void *memcpy(void *d, const void *s, size_t n) {
    char *dst = d; const char *src = s;
    for (size_t i = 0; i < n; i++) dst[i] = src[i];
    return d;
}

static void memset(void *s, int c, size_t n) {
    char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = c;
}

/* Load ELF into an existing process, replacing its address space.
   Returns a new thread that will execute the entry point. */
static thread_t *load_elf_into_process(process_t *proc, const uint8_t *elf_data, size_t elf_size) {
    (void)elf_size;
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)elf_data;

    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F')
        return NULL;
    if (ehdr->e_ident[4] != 2 || ehdr->e_machine != 0x3E)
        return NULL;

    /* Allocate new PML4 for the process */
    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) return NULL;
    uint64_t *pml4 = (uint64_t *)(pml4_phys + hhdm_offset);
    memset(pml4, 0, PAGE_SIZE);

    uint64_t *old_pml4 = (uint64_t *)(kernel_cr3 + hhdm_offset);
    for (int i = 256; i < 512; i++) pml4[i] = old_pml4[i];

    /* Parse program headers and map segments */
    const elf64_phdr_t *phdrs = (const elf64_phdr_t *)(elf_data + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uint64_t virt_start = phdrs[i].p_vaddr & ~0xFFF;
        uint64_t virt_end   = (phdrs[i].p_vaddr + phdrs[i].p_memsz + 0xFFF) & ~0xFFF;

        for (uint64_t vaddr = virt_start; vaddr < virt_end; vaddr += PAGE_SIZE) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) return NULL;
            uint64_t flags = VMM_PRESENT | VMM_USER;
            if (phdrs[i].p_flags & PF_W) flags |= VMM_WRITABLE;
            vmm_map_user(pml4_phys, vaddr, phys, flags, 1);

            uint64_t file_addr = phdrs[i].p_vaddr;
            uint64_t page_start = vaddr;
            uint64_t page_end   = vaddr + PAGE_SIZE;

            if (page_start < file_addr) page_start = file_addr;
            if (page_end > file_addr + phdrs[i].p_filesz)
                page_end = file_addr + phdrs[i].p_filesz;

            if (page_start < page_end) {
                uint64_t copy_off = page_start - phdrs[i].p_vaddr;
                uint64_t copy_len = page_end - page_start;
                void *dest = (void *)(phys + hhdm_offset + (page_start - vaddr));
                memcpy(dest, elf_data + phdrs[i].p_offset + copy_off, copy_len);
            }

            if (page_end < vaddr + PAGE_SIZE) {
                uint64_t zero_start = (page_end > vaddr) ? (page_end - vaddr) : 0;
                void *zptr = (void *)(phys + hhdm_offset + zero_start);
                memset(zptr, 0, PAGE_SIZE - zero_start);
            }
        }
    }

    /* Allocate user stack */
    uint64_t stack_phys = pmm_alloc_page();
    if (!stack_phys) return NULL;
    vmm_map_user(pml4_phys, 0x800000000000, stack_phys,
                 VMM_PRESENT | VMM_USER | VMM_WRITABLE, 1);

    /* Create kernel thread for this process */
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) return NULL;
    void *kstack = kmalloc(4096);
    if (!kstack) { kfree(t); return NULL; }
    t->kernel_stack = (uint64_t)kstack + 4096;
    t->cr3 = pml4_phys;
    t->iopl = 0;
    t->process = proc;

    uint64_t *stk = (uint64_t *)t->kernel_stack - 5;
    stk[0] = 0x23;
    stk[1] = 0x800000000000 + 4096;
    stk[2] = 0x202;
    stk[3] = 0x1B;
    stk[4] = ehdr->e_entry;
    stk -= 2;
    stk[0] = 0; stk[1] = 0;
    stk -= 15;
    for (int j = 0; j < 15; j++) stk[j] = 0;

    t->rsp = (uint64_t)stk;
    t->state = THREAD_STATE_READY;

    /* Update process's page table */
    proc->pml4_phys = pml4_phys;
    proc->main_thread = t;

    return t;
}

int elf_load(const uint8_t *elf_data, size_t elf_size) {
    /* Create a brand new process */
    process_t *proc = kmalloc(sizeof(process_t));
    if (!proc) return -1;
    proc->pid = next_pid++;   /* need access to next_pid from proc.c, we'll expose it later */
    static uint64_t pid_counter = 2; /* hack: we'll use a local static for now */
    proc->pid = pid_counter++;
    proc->next = NULL;
    current_process = proc;   /* set for the new process */

    thread_t *t = load_elf_into_process(proc, elf_data, elf_size);
    if (!t) { kfree(proc); return -1; }
    scheduler_add_thread(t);
    return proc->pid;
}

void sys_exec(const uint8_t *elf_data, size_t elf_size) {
    if (!current_process) return;
    thread_t *new_thread = load_elf_into_process(current_process, elf_data, elf_size);
    if (!new_thread) return;
    scheduler_add_thread(new_thread);
    /* The old thread will exit; the new thread will take over the process. */
    thread_exit();
}