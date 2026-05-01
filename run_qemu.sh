#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"

# Version of Limine to vendor (binary branch)
LIMINE_BRANCH="v8.x-binary"
LIMINE_DIR="$SCRIPT_DIR/limine"

# Download and vendor the Limine binary release if necessary
if ! [ -f "$LIMINE_DIR/limine" ] || ! [ -f "$LIMINE_DIR/limine-bios.sys" ]; then
    echo "=== Vendoring Limine from branch '$LIMINE_BRANCH' ==="
    mkdir -p "$LIMINE_DIR"
    
    TMPDIR=$(mktemp -d)
    git clone https://github.com/limine-bootloader/limine.git \
        --branch="$LIMINE_BRANCH" --depth=1 "$TMPDIR"

    # Copy the files we need
    for f in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin; do
        cp "$TMPDIR/$f" "$LIMINE_DIR/"
    done

    # Build the limine installer tool
    make -C "$TMPDIR" limine
    cp "$TMPDIR/limine" "$LIMINE_DIR/"
    chmod +x "$LIMINE_DIR/limine"

    rm -rf "$TMPDIR"
    echo "=== Limine vendored successfully ==="
fi

echo "=== Building kernel ==="
make clean
make

echo "=== Preparing ISO ==="
rm -rf iso_root orchid.iso
mkdir -p iso_root/limine

# Copy kernel to ISO root
cp kernel.elf iso_root/

# Copy vendored Limine boot files
cp "$LIMINE_DIR/limine-bios.sys" iso_root/limine/
cp "$LIMINE_DIR/limine-bios-cd.bin" iso_root/limine/
cp "$LIMINE_DIR/limine-uefi-cd.bin" iso_root/limine/

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

# Install Limine onto the ISO
"$LIMINE_DIR/limine" bios-install orchid.iso

echo "=== Booting Orchid ==="
qemu-system-x86_64 \
    -cdrom orchid.iso \
    -serial stdio \
    -m 1G \
    -no-reboot \
    -no-shutdown