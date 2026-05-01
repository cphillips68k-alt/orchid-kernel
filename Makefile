CROSS ?=
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS = -std=c11 -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -mcmodel=kernel -O2 -pipe -Wall -Wextra -I. -fno-pic -fno-pie

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000 -no-pie

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
        src/vmm.c \
        src/tss.c \
        src/ipc.c \
        src/bus.c \
        src/syscalls.c \
        src/user.c

ASM_SRC = src/isr.S \
          src/switch.S \
          src/tssflush.S \
          src/syscall_entry.S

C_OBJ = $(C_SRC:.c=.o)
ASM_OBJ = $(ASM_SRC:.S=.o)
OBJS = $(C_OBJ) $(ASM_OBJ)

KERNEL = kernel.elf

.PHONY: all clean run

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.S
	@echo "  AS    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

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