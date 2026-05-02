#ifndef PMM_H
#define PMM_H
#include <stdint.h>
#include <stddef.h>

/* Page size */
#define PAGE_SIZE 4096

/* Initialize the physical memory manager */
void pmm_init(void);

/* Allocate a single physical page, returns physical address */
uint64_t pmm_alloc_page(void);

/* Free a physical page */
void pmm_free_page(uint64_t phys_addr);

/* Get total number of free pages */
uint64_t pmm_free_page_count(void);

#endif