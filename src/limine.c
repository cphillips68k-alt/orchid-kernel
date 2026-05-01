#include "limine.h"

volatile struct limine_framebuffer_request __attribute__((section(".limine_requests"))) 
    framebuffer_request = { LIMINE_FRAMEBUFFER_REQUEST, 0, 0 };
volatile struct limine_memmap_request __attribute__((section(".limine_requests"))) 
    memmap_request = { LIMINE_MEMMAP_REQUEST, 0, 0 };
volatile struct limine_hhdm_request __attribute__((section(".limine_requests"))) 
    hhdm_request = { LIMINE_HHDM_REQUEST, 0, 0 };
volatile struct limine_rsdp_request __attribute__((section(".limine_requests"))) 
    rsdp_request = { LIMINE_RSDP_REQUEST, 0, 0 };
volatile struct limine_kernel_file_request __attribute__((section(".limine_requests"))) 
    kernel_file_request = { LIMINE_KERNEL_FILE_REQUEST, 0, 0 };

__attribute__((section(".limine_requests_start_marker")))
volatile void *volatile limine_requests_start_marker = NULL;

__attribute__((section(".limine_requests_end_marker")))
volatile void *volatile limine_requests_end_marker = NULL;

__attribute__((section(".limine_kernel_info")))
struct limine_kernel_info kernel_info = {
    .magic = LIMINE_KERNEL_INFO_MAGIC,
    .revision = 0,
    .requests_start = &limine_requests_start_marker,
    .requests_end = &limine_requests_end_marker
};