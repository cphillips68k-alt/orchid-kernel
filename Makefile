# Orchid Microkernel Makefile
# Zero dependencies beyond GCC + binutils + Limine

ARCH ?= x86_64

ifeq ($(ARCH),x86_64)
    CC = x86_64-elf-gcc
    LD = x86_64-elf-ld
    OBJCOPY = x86_64-elf-objcopy
    CFLAGS = -std=c11 -ffreestanding -mno-red-zone -mno-mmx -mno-sse \
             -mno-sse2 -Wall -Wextra -O2 -g -pipe \
             -mcmodel=kernel -mgeneral-regs-only
    LDFLAGS = -nostdlib -T linker.ld -static
else
    $(error Unsupported architecture: $(ARCH))
endif

KERNEL_BIN = kernel.elf
ISO_DIR = iso_root
LIMINE_DIR = limine

.PHONY: all clean run

all: $(KERNEL_BIN)

# Source files
SRC = src/main.c src/serial.c src/console.c
OBJ = $(SRC:.c=.o)

%.o: %.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJ) linker.ld
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -f $(OBJ) $(KERNEL_BIN)
	rm -rf $(ISO_DIR)

# Run in QEMU
run: $(KERNEL_BIN)
	qemu-system-x86_64 \
		-kernel $(KERNEL_BIN) \
		-serial stdio \
		-m 1G \
		-no-reboot \
		-no-shutdown