/* BSD Zero Clause License (0BSD) */
#ifndef LIMINE_H
#define LIMINE_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Misc */

#ifdef LIMINE_NO_POINTERS
#  define LIMINE_PTR(TYPE) uint64_t
#else
#  define LIMINE_PTR(TYPE) TYPE
#endif

#define LIMINE_BASE_REVISION(N) \
    uint64_t limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t d[8];
};

#define LIMINE_MEDIA_TYPE_GENERIC 0
#define LIMINE_MEDIA_TYPE_OPTICAL 1
#define LIMINE_MEDIA_TYPE_TFTP 2

struct limine_file {
    uint64_t revision;
    LIMINE_PTR(void *) address;
    uint64_t size;
    LIMINE_PTR(char *) path;
    LIMINE_PTR(char *) cmdline;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};

/* Boot info */

#define LIMINE_BOOT_INFO_REQUEST { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }

struct limine_boot_info_response {
    uint64_t revision;
    LIMINE_PTR(char *) name;
    LIMINE_PTR(char *) version;
};

struct limine_boot_info_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_boot_info_response *) response;
};

/* Stack size */

#define LIMINE_STACK_SIZE_REQUEST { LIMINE_COMMON_MAGIC, 0x224ef0460a8e8926, 0xe1cb0fc25f46ea3d }

struct limine_stack_size_response {
    uint64_t revision;
};

struct limine_stack_size_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_stack_size_response *) response;
    uint64_t stack_size;
};

/* HHDM */

#define LIMINE_HHDM_REQUEST { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_hhdm_response *) response;
};

/* Forward declare terminal struct before it's used in the callback typedef */
struct limine_terminal;

/* Framebuffer */

#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }

#define LIMINE_FRAMEBUFFER_RGB 1

struct limine_framebuffer {
    LIMINE_PTR(void *) address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    LIMINE_PTR(void *) edid;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    LIMINE_PTR(struct limine_framebuffer **) framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_framebuffer_response *) response;
};

/* Terminal (now the struct is already forward-declared) */

#define LIMINE_TERMINAL_REQUEST { LIMINE_COMMON_MAGIC, 0xc8ac59310c2b0844, 0xa68d0c7265d38878 }

#define LIMINE_TERMINAL_CB_DEC 10
#define LIMINE_TERMINAL_CB_BELL 20
#define LIMINE_TERMINAL_CB_PRIVATE_ID 30
#define LIMINE_TERMINAL_CB_STATUS_REPORT 40
#define LIMINE_TERMINAL_CB_POS_REPORT 50
#define LIMINE_TERMINAL_CB_KBD_LEDS 60
#define LIMINE_TERMINAL_CB_MODE 70
#define LIMINE_TERMINAL_CB_LINUX 80

#define LIMINE_TERMINAL_CTX_SIZE ((uint64_t)(-1))
#define LIMINE_TERMINAL_CTX_SAVE ((uint64_t)(-2))
#define LIMINE_TERMINAL_CTX_RESTORE ((uint64_t)(-3))
#define LIMINE_TERMINAL_FULL_REFRESH ((uint64_t)(-4))

typedef void (*limine_terminal_callback)(struct limine_terminal *, uint64_t, uint64_t, uint64_t, uint64_t);

struct limine_terminal {
    uint64_t columns;
    uint64_t rows;
    LIMINE_PTR(struct limine_terminal_response **) write;
    LIMINE_PTR(void *) rsvd[7];
};

struct limine_terminal_response {
    uint64_t revision;
    uint64_t terminal_count;
    LIMINE_PTR(struct limine_terminal **) terminals;
    LIMINE_PTR(limine_terminal_callback) callback;
};

struct limine_terminal_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_terminal_response *) response;
    LIMINE_PTR(limine_terminal_callback) callback;
};

/* Rest of the file unchanged (SMP, memory map, kernel file, etc.) – the same as I provided earlier */
...