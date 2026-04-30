#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"

echo "=== Building kernel ==="
make clean
make

echo "=== Setting up Limine ==="

# Clone Limine if we don't have it
if [ ! -d "limine" ]; then
    git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1
fi

# Build Limine
cd limine
make
cd ..

# Create ISO directory
rm -rf iso_root
mkdir -p iso_root/boot

# Copy kernel and config
cp kernel.elf iso_root/boot/

cat > iso_root/boot/limine.conf << 'EOF'
TIMEOUT=0
VERBOSE=yes

/Orchid Microkernel
    PROTOCOL=limine
    KERNEL_PATH=boot:///boot/kernel.elf
EOF

# Copy Limine boot files
cp limine/limine-bios.sys iso_root/boot/
cp limine/limine-bios-cd.bin iso_root/boot/
cp limine/limine-uefi-cd.bin iso_root/boot/

# Create ISO
echo "=== Creating bootable ISO ==="
xorriso -as mkisofs -b boot/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot boot/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    iso_root -o orchid.iso

# Deploy Limine onto the ISO
./limine/limine bios-install orchid.iso

echo "=== Booting Orchid ==="
qemu-system-x86_64 \
    -cdrom orchid.iso \
    -serial stdio \
    -m 1G \
    -no-reboot \
    -no-shutdown