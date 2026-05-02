CROSS ?=
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS = -std=c11 -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -mcmodel=kernel -O2 -pipe -Wall -Wextra \
         -I src/kernel -I src/user -I . -fno-pic -fno-pie

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000 -no-pie

C_SRC = src/kernel/main.c \
        src/kernel/serial.c \
        src/kernel/console.c \
        src/kernel/limine.c \
        src/kernel/gdt.c \
        src/kernel/idt.c \
        src/kernel/isr_handler.c \
        src/kernel/scheduler.c \
        src/kernel/pit.c \
        src/kernel/pmm.c \
        src/kernel/vmm.c \
        src/kernel/tss.c \
        src/kernel/ipc.c \
        src/kernel/bus.c \
        src/kernel/syscalls.c \
        src/kernel/timer.c \
        src/kernel/user.c \
        src/kernel/proc.c \
        src/kernel/irq.c \
        src/kernel/elf.c

USER_SRC = src/user/kbd.c \
           src/user/vfs.c

ASM_SRC = src/kernel/isr.S \
          src/kernel/switch.S \
          src/kernel/tssflush.S \
          src/kernel/syscall_entry.S

C_OBJ = $(C_SRC:.c=.o)
USER_OBJ = $(USER_SRC:.c=.o)
ASM_OBJ = $(ASM_SRC:.S=.o)
OBJS = $(C_OBJ) $(USER_OBJ) $(ASM_OBJ) src/user/init.o

KERNEL = kernel.elf

.PHONY: all clean run

all: $(KERNEL)

src/user/init.bin: src/user/init.c
	@echo "  CC    $< (user)"
	@$(CC) -ffreestanding -nostdlib -static -o src/user/init.elf $<
	@$(OBJCOPY) -O binary src/user/init.elf $@

src/user/init.o: src/user/init.bin
	@echo "  OBJCOPY $<"
	@cd src/user && $(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 init.bin init.o

$(KERNEL): $(OBJS) linker.ld
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

src/kernel/%.o: src/kernel/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

src/user/%.o: src/user/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

src/kernel/%.o: src/kernel/%.S
	@echo "  AS    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

DEPENDS = $(OBJS:.o=.d)
-include $(DEPENDS)

src/kernel/%.d: src/kernel/%.c
	@$(CC) $(CFLAGS) -MM -MT '$(<:.c=.o)' $< > $@

src/user/%.d: src/user/%.c
	@$(CC) $(CFLAGS) -MM -MT '$(<:.c=.o)' $< > $@

src/kernel/%.d: src/kernel/%.S
	@$(CC) $(CFLAGS) -MM -MT '$(<:.S=.o)' $< > $@

clean:
	rm -f $(OBJS) $(DEPENDS) $(KERNEL) src/user/init.elf src/user/init.bin src/user/init.o

run:
	./run_qemu.sh