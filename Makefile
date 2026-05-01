CROSS ?=
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS = -std=c11 -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -mcmodel=kernel -O2 -pipe -Wall -Wextra -I. -fno-pic -fno-pie

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000 -no-pie

# Source files
C_SRC = src/main.c \
        src/serial.c \
        src/console.c \
        src/limine.c \
        src/gdt.c \
        src/idt.c \
        src/isr_handler.c \
        src/scheduler.c \
        src/pit.c \
        src/pmm.c \
        src/vmm.c

ASM_SRC = src/isr.S \
          src/switch.S

# Object files
C_OBJ = $(C_SRC:.c=.o)
ASM_OBJ = $(ASM_SRC:.S=.o)
OBJS = $(C_OBJ) $(ASM_OBJ)

# Target
KERNEL = kernel.elf

.PHONY: all clean run

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

# C files
src/%.o: src/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assembly files
src/%.o: src/%.S
	@echo "  AS    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Dependency files
DEPENDS = $(OBJS:.o=.d)
-include $(DEPENDS)

src/%.d: src/%.c
	@$(CC) $(CFLAGS) -MM -MT '$(<:.c=.o)' $< > $@

src/%.d: src/%.S
	@$(CC) $(CFLAGS) -MM -MT '$(<:.S=.o)' $< > $@

clean:
	rm -f $(OBJS) $(DEPENDS) $(KERNEL)

run:
	./run_qemu.sh