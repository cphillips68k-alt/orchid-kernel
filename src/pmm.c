#include "pmm.h"
#include "limine.h"
#include "sync.h"
#include "serial.h"

/* External Limine memmap response */
extern volatile struct limine_memmap_request memmap_request;

/* HHDM offset (set by main.c before pmm_init) */
extern uint64_t hhdm_offset;

#define MAX_FREE_LIST 300000   /* enough for ~1.2 GB of RAM */

static uint64_t free_stack[MAX_FREE_LIST];
static int free_top = 0;
static spinlock_t pmm_lock = 0;

void pmm_init(void) {
    struct limine_memmap_response *mm_resp =
        (struct limine_memmap_response *)memmap_request.response;
    if (!mm_resp) {
        serial_write("[PMM] No memory map from Limine!\n");
        return;
    }

    uint64_t total_free = 0;

    for (uint64_t i = 0; i < mm_resp->entry_count; i++) {
        struct limine_memmap_entry *entry = mm_resp->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;

        /* Align base up to page boundary */
        uint64_t base = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t end = (entry->base + entry->length) & ~(PAGE_SIZE - 1);

        while (base < end) {
            if (free_top < MAX_FREE_LIST) {
                free_stack[free_top++] = base;
                total_free++;
            } else {
                serial_write("[PMM] Free list overflow, some memory lost\n");
                break;
            }
            base += PAGE_SIZE;
        }
    }

    serial_printf("[PMM] Initialized, %d free pages (%d MB)\n",
                  total_free, (total_free * 4) / 1024);
}

uint64_t pmm_alloc_page(void) {
    spin_lock(&pmm_lock);
    if (free_top == 0) {
        spin_unlock(&pmm_lock);
        return 0;  /* out of memory */
    }
    uint64_t page = free_stack[--free_top];
    spin_unlock(&pmm_lock);
    return page;
}

void pmm_free_page(uint64_t phys_addr) {
    spin_lock(&pmm_lock);
    if (free_top < MAX_FREE_LIST) {
        free_stack[free_top++] = phys_addr;
    }
    spin_unlock(&pmm_lock);
}

uint64_t pmm_free_page_count(void) {
    spin_lock(&pmm_lock);
    uint64_t count = free_top;
    spin_unlock(&pmm_lock);
    return count;
}