# Orchid Microkernel

A hobby x86_64 microkernel written in C. Boots via the Limine 8.x boot protocol.

> **License**: GPLv3+ – see the license section below.

## Architecture

Orchid follows true microkernel principles:
- **Kernel space**: Memory management, scheduling, IPC, syscall interface
- **User space**: All drivers and services run as separate processes
- **Singleton bus**: All inter-process communication goes through a central
  named‑port registry – no direct P2P messaging between services

## Current Status (v0.3.0)

**Kernel (running in Ring 0):**
- Physical memory manager (page frame allocator)
- Virtual memory manager (page tables, kernel heap)
- Preemptive round‑robin scheduler
- Synchronous IPC (send/receive, blocking)
- Singleton bus (named service registry)
- Process model with `fork()` (deep copy of address space)
- IRQ‑to‑IPC forwarding (drivers receive IRQs as bus messages)
- POSIX‑style syscall interface (`write`, `read`, `open`, `close`, `exit`,
  `sleep`, `nanosleep`, `fork`, `iopl`, `irq_register`, `yield`)
- TSS with per‑thread I/O port bitmap
- PS/2 keyboard driver (IRQ1 → IPC forwarding)
- Virtual File System service (fd management, read/write)
- Framebuffer console with built‑in font

**User space (running in Ring 3):**
- Basic user‑mode thread (prints "Hello from user!" via syscall)
- VFS and keyboard drivers are ready to move to user space
  (currently run as kernel threads, communicating via the bus)

## Roadmap

1. **ELF loader** – parse and load user‑space executables
2. **`exec()` syscall** – replace process image
3. **Separate user binaries** – VFS, kbd, init compiled as user programs
4. **User library** – syscall wrappers, IPC, bus, string functions
5. **Shell** – interactive command prompt
6. **Disk driver** – block device service (IDE/AHCI)
7. **File system** – ext2 or FAT service
8. **SMP** – multi‑core support

## Building

Requirements:
- GCC (system GCC works with `-ffreestanding -fno-pic`)
- GNU binutils
- QEMU for testing
- `git` and `xorriso`

```bash
chmod +x run_qemu.sh
./run_qemu.sh   # Boots the kernel in QEMU