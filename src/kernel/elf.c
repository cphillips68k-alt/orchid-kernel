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
#define PF_R    4
#define PF_W    2
#define PF_X    1

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

static thread_t *load_elf_into_process(process_t *proc, const uint8_t *elf_data, size_t elf_size) {
    (void)elf_size;
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)elf_data;

    /* Check ELF magic */
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        serial_write("[ELF] Bad magic\n");
        return NULL;
    }

    /* Check 64-bit little-endian x86-64 */
    if (ehdr->e_ident[4] != 2 || ehdr->e_machine != 0x3E) {
        serial_write("[ELF] Not 64-bit x86-64\n");
        return NULL;
    }

    serial_printf("[ELF] Entry point: 0x%x\n", ehdr->e_entry);
    serial_printf("[ELF] Program headers: %d at offset 0x%x\n", ehdr->e_phnum, ehdr->e_phoff);

    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        serial_write("[ELF] Failed to allocate PML4\n");
        return NULL;
    }

    uint64_t *pml4 = (uint64_t *)(pml4_phys + hhdm_offset);
    memset(pml4, 0, PAGE_SIZE);

    uint64_t *old_pml4 = (uint64_t *)(kernel_cr3 + hhdm_offset);
    for (int i = 256; i < 512; i++) pml4[i] = old_pml4[i];

    const elf64_phdr_t *phdrs = (const elf64_phdr_t *)(elf_data + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uint64_t virt_start = phdrs[i].p_vaddr & ~0xFFF;
        uint64_t virt_end   = (phdrs[i].p_vaddr + phdrs[i].p_memsz + 0xFFF) & ~0xFFF;

        serial_printf("[ELF] Loading segment: vaddr=0x%x memsz=0x%x filesz=0x%x\n",
                      phdrs[i].p_vaddr, phdrs[i].p_memsz, phdrs[i].p_filesz);

        for (uint64_t vaddr = virt_start; vaddr < virt_end; vaddr += PAGE_SIZE) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) {
                serial_write("[ELF] Out of memory\n");
                return NULL;
            }

            uint64_t flags = VMM_PRESENT | VMM_USER;
            if (phdrs[i].p_flags & PF_W) flags |= VMM_WRITABLE;
            vmm_map_user(pml4_phys, vaddr, phys, flags, 1);

            uint64_t file_addr   = phdrs[i].p_vaddr;
            uint64_t page_start  = vaddr;
            uint64_t page_end    = vaddr + PAGE_SIZE;

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
    if (!stack_phys) {
        serial_write("[ELF] Failed to allocate user stack\n");
        return NULL;
    }
    vmm_map_user(pml4_phys, 0x800000000000, stack_phys,
                 VMM_PRESENT | VMM_USER | VMM_WRITABLE, 1);

    /* Create thread for this process */
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) {
        serial_write("[ELF] Failed to allocate thread\n");
        return NULL;
    }
    void *kstack = kmalloc(4096);
    if (!kstack) {
        kfree(t);
        serial_write("[ELF] Failed to allocate kernel stack\n");
        return NULL;
    }

    t->kernel_stack = (uint64_t)kstack + 4096;
    t->cr3 = pml4_phys;
    t->iopl = 0;
    t->process = proc;

    /* Build iret frame to return to user mode at entry point */
    uint64_t *stk = (uint64_t *)t->kernel_stack - 5;
    stk[0] = 0x23;                     /* SS  = user data seg */
    stk[1] = 0x800000000000 + 4096;   /* RSP = top of stack */
    stk[2] = 0x202;                   /* RFLAGS */
    stk[3] = 0x1B;                    /* CS  = user code seg */
    stk[4] = ehdr->e_entry;           /* RIP = entry point */
    stk -= 2;
    stk[0] = 0; stk[1] = 0;
    stk -= 15;
    for (int j = 0; j < 15; j++) stk[j] = 0;

    t->rsp = (uint64_t)stk;
    t->state = THREAD_STATE_READY;

    proc->pml4_phys = pml4_phys;
    proc->main_thread = t;

    serial_write("[ELF] Process loaded successfully\n");
    return t;
}

int elf_load(const uint8_t *elf_data, size_t elf_size) {
    serial_printf("[ELF] Loading binary: size=%d\n", elf_size);

    process_t *proc = kmalloc(sizeof(process_t));
    if (!proc) {
        serial_write("[ELF] Failed to allocate process\n");
        return -1;
    }

    proc->pid = proc_alloc_pid();
    proc->next = NULL;
    current_process = proc;

    thread_t *t = load_elf_into_process(proc, elf_data, elf_size);
    if (!t) {
        kfree(proc);
        return -1;
    }

    scheduler_add_thread(t);
    return (int)proc->pid;
}

void sys_exec(const uint8_t *elf_data, size_t elf_size) {
    if (!current_process) return;
    thread_t *new_thread = load_elf_into_process(current_process, elf_data, elf_size);
    if (!new_thread) return;

    current_process->main_thread = new_thread;
    scheduler_add_thread(new_thread);
    thread_exit();
}