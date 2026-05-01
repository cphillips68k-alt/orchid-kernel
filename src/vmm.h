#ifndef VMM_H
#define VMM_H
#include <stdint.h>
#include <stddef.h>

/* Flags for vmm_map */
#define VMM_PRESENT  0x001
#define VMM_WRITABLE 0x002
#define VMM_USER     0x004
#define VMM_NX       0x8000000000000000ULL  /* No-execute (not used yet) */

void vmm_init(void);
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t virt);

/* Simple kernel heap */
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif