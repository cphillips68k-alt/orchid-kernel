CC = gcc
LD = ld

CFLAGS = -std=c11 -ffreestanding -mno-red-zone -mno-mmx -mno-sse \
         -mno-sse2 -Wall -Wextra -O2 -g -pipe \
         -mcmodel=kernel -mgeneral-regs-only -nostdinc -fno-PIE \
		 -fno-pic -fno-pie

LDFLAGS = -nostdlib -T linker.ld -static -no-pie

KERNEL_BIN = kernel.elf

.PHONY: all clean run

all: $(KERNEL_BIN)

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

run: $(KERNEL_BIN)
	qemu-system-x86_64 \
		-kernel $(KERNEL_BIN) \
		-serial stdio \
		-m 1G \
		-no-reboot \
		-no-shutdown