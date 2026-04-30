/* BSD Zero Clause License (0BSD) - vendored from limine-bootloader/limine */
#ifndef LIMINE_H
#define LIMINE_H

#include <stdint.h>

/* Request/response IDs - must be reversed for 64-bit */
#define LIMINE_FRAMEBUFFER_REQUEST       {0x9d5827dcd881dd75, 0xa3148604f6fab11b}
#define LIMINE_MEMMAP_REQUEST            {0x67cf3d9d378a806f, 0xe304acdfc50c3c62}
#define LIMINE_RSDP_REQUEST              {0xc5e77b6b397e7b43, 0x27637845accdcf3}
#define LIMINE_SMP_REQUEST               {0xf20235aa2f8c8e7f, 0x6b0c6bcd28a0e729}
#define LIMINE_BOOT_TIME_REQUEST         {0x502746e184c088aa, 0xfbc5ec83e6327893}
#define LIMINE_EFI_SYSTEM_TABLE_REQUEST  {0x5ceba5163eaaf6d6, 0x0a6981610cf65fcc}
#define LIMINE_KERNEL_FILE_REQUEST       {0xad97e90e83f1ed67, 0x31eb5d1c5ff23b69}
#define LIMINE_MODULE_REQUEST            {0x3e7e279702be32af, 0xca1c4f3bd1280cee}
#define LIMINE_HHDM_REQUEST              {0x48dcf1cb8ad2b852, 0x63984e959a98244b}

/* Generic request/response header */
struct limine_request {
    uint64_t id[4];
    uint64_t revision;
    void    *response;
};

/* Framebuffer */
struct limine_framebuffer {
    void     *address;
    uint64_t  width;
    uint64_t  height;
    uint64_t  pitch;
    uint16_t  bpp;
    uint8_t   memory_model;
    uint8_t   red_mask_size;
    uint8_t   red_mask_shift;
    uint8_t   green_mask_size;
    uint8_t   green_mask_shift;
    uint8_t   blue_mask_size;
    uint8_t   blue_mask_shift;
};

struct limine_framebuffer_response {
    uint64_t               revision;
    uint64_t               framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    void    *response;
};

/* Memory map */
struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_memmap_response {
    uint64_t                    revision;
    uint64_t                    entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    void    *response;
};

/* HHDM */
struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    void    *response;
};

/* RSDP */
struct limine_rsdp_response {
    uint64_t revision;
    void    *address;
};

struct limine_rsdp_request {
    uint64_t id[4];
    uint64_t revision;
    void    *response;
};

/* SMP */
struct limine_smp_info;

typedef void (*limine_goto_address)(struct limine_smp_info *);

struct limine_smp_info {
    uint32_t             processor_id;
    uint32_t             lapic_id;
    uint64_t             reserved;
    limine_goto_address  goto_address;
    uint64_t             extra_argument;
};

struct limine_smp_response {
    uint64_t               revision;
    uint32_t               flags;
    uint32_t               bsp_lapic_id;
    uint64_t               cpu_count;
    struct limine_smp_info **cpus;
};

/* Kernel file */
struct limine_file {
    uint64_t revision;
    void    *address;
    uint64_t size;
    char    *path;
    char    *cmdline;
};

struct limine_kernel_file_response {
    uint64_t            revision;
    struct limine_file *kernel_file;
};

struct limine_kernel_file_request {
    uint64_t id[4];
    uint64_t revision;
    void    *response;
};

/* These are set up before _start, Limine fills them in */
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_kernel_file_request kernel_file_request;

extern void *limine_requests_start_marker;
extern void *limine_requests_end_marker;

#endif