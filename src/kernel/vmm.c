#include "vmm.h"
#include "pmm.h"
#include "sync.h"
#include "serial.h"
#include <stddef.h>

extern uint64_t hhdm_offset;

#define PML4_INDEX(v) (((v) >> 39) & 0x1FF)
#define PDPT_INDEX(v) (((v) >> 30) & 0x1FF)
#define PD_INDEX(v)   (((v) >> 21) & 0x1FF)
#define PT_INDEX(v)   (((v) >> 12) & 0x1FF)

static uint64_t *pml4;
static spinlock_t vmm_lock = 0;

/* ----- FIX: entry_get_addr now returns the full physical address, not the PFN ----- */
static inline uint64_t entry_get_addr(uint64_t entry) {
    return entry & 0x000FFFFFFFFFF000;      /* no shift! */
}

static inline uint64_t make_entry(uint64_t phys, uint64_t flags) {
    return (phys & 0x000FFFFFFFFFF000) | (flags & 0xFFF);
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

void vmm_init(void) {
    serial_write("[VMM] Initializing...\n");
    __asm__ volatile ("mov %%cr3, %0" : "=r"(pml4));
    serial_printf("[VMM] PML4 at phys %x\n", (uint64_t)pml4);
}

/* Only called with vmm_lock held */
static void ensure_page_table(uint64_t *entry_ptr) {
    if (!(*entry_ptr & 1)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (pt_phys == 0) {
            serial_write("[VMM] Out of memory\n");
            return;
        }
        uint64_t *pt_virt = phys_to_virt(pt_phys);
        for (int i = 0; i < 512; i++) pt_virt[i] = 0;
        *entry_ptr = make_entry(pt_phys, VMM_PRESENT | VMM_WRITABLE);
    }
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (phys == 0) {
        serial_printf("[VMM] ERROR: vmm_map called with phys=0 for virt %x\n", virt);
        return;
    }

    spin_lock(&vmm_lock);
    uint64_t *pml4_virt = phys_to_virt((uint64_t)pml4);

    int pml4_i = PML4_INDEX(virt);
    ensure_page_table(&pml4_virt[pml4_i]);
    if (!(pml4_virt[pml4_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pdpt = phys_to_virt(entry_get_addr(pml4_virt[pml4_i]));
    int pdpt_i = PDPT_INDEX(virt);
    ensure_page_table(&pdpt[pdpt_i]);
    if (!(pdpt[pdpt_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pd = phys_to_virt(entry_get_addr(pdpt[pdpt_i]));
    int pd_i = PD_INDEX(virt);
    ensure_page_table(&pd[pd_i]);
    if (!(pd[pd_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pt = phys_to_virt(entry_get_addr(pd[pd_i]));
    int pt_i = PT_INDEX(virt);

    pt[pt_i] = make_entry(phys, flags | VMM_PRESENT);

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    spin_unlock(&vmm_lock);
}

void vmm_unmap(uint64_t virt) {
    spin_lock(&vmm_lock);
    uint64_t *pml4_virt = phys_to_virt((uint64_t)pml4);

    int pml4_i = PML4_INDEX(virt);
    if (!(pml4_virt[pml4_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pdpt = phys_to_virt(entry_get_addr(pml4_virt[pml4_i]));
    int pdpt_i = PDPT_INDEX(virt);
    if (!(pdpt[pdpt_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pd = phys_to_virt(entry_get_addr(pdpt[pdpt_i]));
    int pd_i = PD_INDEX(virt);
    if (!(pd[pd_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pt = phys_to_virt(entry_get_addr(pd[pd_i]));
    int pt_i = PT_INDEX(virt);
    pt[pt_i] = 0;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    spin_unlock(&vmm_lock);
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    spin_lock(&vmm_lock);
    uint64_t *pml4_virt = phys_to_virt((uint64_t)pml4);
    uint64_t phys = 0;

    int pml4_i = PML4_INDEX(virt);
    if (!(pml4_virt[pml4_i] & 1)) goto out;
    uint64_t *pdpt = phys_to_virt(entry_get_addr(pml4_virt[pml4_i]));
    int pdpt_i = PDPT_INDEX(virt);
    if (!(pdpt[pdpt_i] & 1)) goto out;
    uint64_t *pd = phys_to_virt(entry_get_addr(pdpt[pdpt_i]));
    int pd_i = PD_INDEX(virt);
    if (!(pd[pd_i] & 1)) goto out;
    uint64_t *pt = phys_to_virt(entry_get_addr(pd[pd_i]));
    int pt_i = PT_INDEX(virt);
    if (!(pt[pt_i] & 1)) goto out;

    phys = (entry_get_addr(pt[pt_i])) | (virt & 0xFFF);   /* entry_get_addr already gives full phys */
out:
    spin_unlock(&vmm_lock);
    return phys;
}

void vmm_map_user(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags, int is_user) {
    if (phys == 0) return;

    spin_lock(&vmm_lock);
    uint64_t *pml4_virt = phys_to_virt(pml4_phys);

    int pml4_i = PML4_INDEX(virt);
    ensure_page_table(&pml4_virt[pml4_i]);
    if (!(pml4_virt[pml4_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pdpt = phys_to_virt(entry_get_addr(pml4_virt[pml4_i]));
    int pdpt_i = PDPT_INDEX(virt);
    ensure_page_table(&pdpt[pdpt_i]);
    if (!(pdpt[pdpt_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pd = phys_to_virt(entry_get_addr(pdpt[pdpt_i]));
    int pd_i = PD_INDEX(virt);
    ensure_page_table(&pd[pd_i]);
    if (!(pd[pd_i] & 1)) { spin_unlock(&vmm_lock); return; }

    uint64_t *pt = phys_to_virt(entry_get_addr(pd[pd_i]));
    int pt_i = PT_INDEX(virt);
    uint64_t entry = make_entry(phys, flags | VMM_PRESENT | (is_user ? VMM_USER : 0));
    pt[pt_i] = entry;

    spin_unlock(&vmm_lock);
}

uint64_t user_virt_to_phys(uint64_t pml4_phys, uint64_t virt) {
    spin_lock(&vmm_lock);
    uint64_t *pml4_virt = phys_to_virt(pml4_phys);

    int pml4_i = PML4_INDEX(virt);
    if (!(pml4_virt[pml4_i] & 1)) { spin_unlock(&vmm_lock); return 0; }

    uint64_t *pdpt = phys_to_virt(entry_get_addr(pml4_virt[pml4_i]));
    int pdpt_i = PDPT_INDEX(virt);
    if (!(pdpt[pdpt_i] & 1)) { spin_unlock(&vmm_lock); return 0; }

    uint64_t *pd = phys_to_virt(entry_get_addr(pdpt[pdpt_i]));
    int pd_i = PD_INDEX(virt);
    if (!(pd[pd_i] & 1)) { spin_unlock(&vmm_lock); return 0; }

    uint64_t *pt = phys_to_virt(entry_get_addr(pd[pd_i]));
    int pt_i = PT_INDEX(virt);
    if (!(pt[pt_i] & 1)) { spin_unlock(&vmm_lock); return 0; }

    uint64_t phys = entry_get_addr(pt[pt_i]) | (virt & 0xFFF);
    spin_unlock(&vmm_lock);
    return phys;
}

/* --- Kernel heap (unchanged) --- */
#define HEAP_START_VIRT 0xFFFFF00000000000
#define HEAP_INITIAL_PAGES 16

typedef struct heap_block {
    size_t size;
    int free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;
static spinlock_t heap_lock = 0;
static int heap_initialized = 0;
static uint64_t heap_end_virt = HEAP_START_VIRT;

static int heap_expand_locked(size_t min_size) {
    (void)min_size;
    uint64_t phys = pmm_alloc_page();
    if (!phys) return 0;
    vmm_map(heap_end_virt, phys, VMM_PRESENT | VMM_WRITABLE);

    heap_block_t *new_block = (heap_block_t *)heap_end_virt;
    new_block->size = PAGE_SIZE - sizeof(heap_block_t);
    new_block->free = 1;
    new_block->next = NULL;

    if (heap_head == NULL) {
        heap_head = new_block;
    } else {
        heap_block_t *last = heap_head;
        while (last->next) last = last->next;
        if (last->free && (uint64_t)((char*)last + sizeof(heap_block_t) + last->size) == heap_end_virt) {
            last->size += sizeof(heap_block_t) + new_block->size;
        } else {
            last->next = new_block;
        }
    }

    heap_end_virt += PAGE_SIZE;
    return 1;
}

static void heap_init(void) {
    for (int i = 0; i < HEAP_INITIAL_PAGES; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) break;
        vmm_map(HEAP_START_VIRT + i * PAGE_SIZE, phys, VMM_PRESENT | VMM_WRITABLE);
        heap_end_virt = HEAP_START_VIRT + (i + 1) * PAGE_SIZE;
    }
    heap_head = (heap_block_t *)HEAP_START_VIRT;
    heap_head->size = heap_end_virt - HEAP_START_VIRT - sizeof(heap_block_t);
    heap_head->free = 1;
    heap_head->next = NULL;
    heap_initialized = 1;
}

void *kmalloc(size_t size) {
    spin_lock(&heap_lock);
    if (!heap_initialized) heap_init();

    size = (size + 7) & ~7;

    while (1) {
        heap_block_t *curr = heap_head;
        while (curr) {
            if (curr->free && curr->size >= size) {
                if (curr->size >= size + sizeof(heap_block_t) + 8) {
                    heap_block_t *new_block = (heap_block_t *)((char *)(curr + 1) + size);
                    new_block->size = curr->size - size - sizeof(heap_block_t);
                    new_block->free = 1;
                    new_block->next = curr->next;
                    curr->size = size;
                    curr->next = new_block;
                }
                curr->free = 0;
                spin_unlock(&heap_lock);
                return (void *)(curr + 1);
            }
            curr = curr->next;
        }
        if (!heap_expand_locked(size)) {
            spin_unlock(&heap_lock);
            return NULL;
        }
    }
}

void kfree(void *ptr) {
    if (!ptr) return;
    spin_lock(&heap_lock);
    heap_block_t *block = (heap_block_t *)ptr - 1;
    block->free = 1;

    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
    }

    heap_block_t *prev = NULL;
    for (heap_block_t *cur = heap_head; cur; cur = cur->next) {
        if (cur == block) break;
        prev = cur;
    }
    if (prev && prev->free &&
        (uint64_t)((char*)prev + sizeof(heap_block_t) + prev->size) == (uint64_t)block) {
        prev->size += sizeof(heap_block_t) + block->size;
        prev->next = block->next;
    }

    spin_unlock(&heap_lock);
}