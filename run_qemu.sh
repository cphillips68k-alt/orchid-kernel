#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"

echo "=== Building kernel ==="
make clean
make

echo "=== Preparing ISO ==="
rm -rf iso_root orchid.iso
mkdir -p iso_root/limine

# Copy kernel to ISO root
cp kernel.elf iso_root/

# Copy vendored Limine boot files
cp limine/limine-bios.sys iso_root/limine/
cp limine/limine-bios-cd.bin iso_root/limine/
cp limine/limine-uefi-cd.bin iso_root/limine/

# Create Limine config
cat > iso_root/limine.cfg << 'EOF'
TIMEOUT=0
:Orchid Microkernel
PROTOCOL=limine
KERNEL_PATH=boot:///kernel.elf
EOF

# Create bootable ISO
echo "=== Creating bootable ISO ==="
xorriso -as mkisofs \
    -b limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --efi-boot limine/limine-uefi-cd.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    iso_root -o orchid.iso

# Install Limine
./limine/limine bios-install orchid.iso

echo "=== Booting Orchid ==="
qemu-system-x86_64 \
    -cdrom orchid.iso \
    -serial stdio \
    -m 1G \
    -no-reboot \
    -no-shutdown