# Orchid Microkernel (C Rewrite)

A hobby x86_64 microkernel written in C with zero external dependencies.
Boots via the Limine boot protocol.

## Why this exists

The original Rust version worked but pulled in 80+ crates for basic
functionality like serial output and VGA text. This rewrite does everything
with:
- A single vendored Limine header (0BSD licensed)
- The C standard library's freestanding headers (no host OS dependencies)
- Raw inline assembly where needed

No crates. No npm-style dependency tree. No build system beyond a Makefile.

## Building

Requirements:
- GCC cross-compiler targeting `x86_64-elf`
- GNU binutils for the same target
- QEMU for testing

```bash
make          # Builds kernel.elf
make run      # Builds and boots in QEMU