#include "limine.h"
#include <stddef.h>

/* Base revision for all requests */
volatile LIMINE_BASE_REVISION(3);

/* Request declarations */
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 1
};

volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 2
};

volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 1
};

volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 1
};

volatile struct limine_kernel_file_request kernel_file_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 1
};

/* Start/end markers */
volatile LIMINE_REQUESTS_START_MARKER;
volatile LIMINE_REQUESTS_END_MARKER;

/* Kernel info structure */
volatile struct limine_kernel_info kernel_info = {
    .magic = LIMINE_KERNEL_INFO_MAGIC,
    .revision = 3,
    .requests_start = (void *)(uintptr_t)&limine_requests_start_marker,
    .requests_end   = (void *)(uintptr_t)&limine_requests_end_marker
};