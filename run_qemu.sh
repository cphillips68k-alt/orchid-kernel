#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$SCRIPT_DIR"

# Version of Limine to download (stable release)
LIMINE_VERSION="8.4.0"
LIMINE_DIR="$SCRIPT_DIR/limine"
LIMINE_BIN="$LIMINE_DIR/limine"
LIMINE_FILES=(
    "limine-bios.sys"
    "limine-bios-cd.bin"
    "limine-uefi-cd.bin"
)

# Download and vendor Limine if necessary
if ! [ -f "$LIMINE_BIN" ] || ! [ -f "$LIMINE_DIR/limine-bios.sys" ]; then
    echo "=== Vendoring Limine $LIMINE_VERSION ==="
    mkdir -p "$LIMINE_DIR"
    # Fetch binary release from GitHub
    LIMINE_URL="https://github.com/limine-bootloader/limine/releases/download/v${LIMINE_VERSION}/limine-${LIMINE_VERSION}-binary.tar.xz"
    ARCHIVE="$LIMINE_DIR/limine-binary.tar.xz"

    if command -v wget &>/dev/null; then
        wget -O "$ARCHIVE" "$LIMINE_URL"
    elif command -v curl &>/dev/null; then
        curl -L -o "$ARCHIVE" "$LIMINE_URL"
    else
        echo "Error: wget or curl is required to download Limine."
        exit 1
    fi

    # Extract the tarball -- it contains a top-level directory like 'limine-8.4.0-binary/'
    TMPDIR="$LIMINE_DIR/tmp_limine"
    mkdir -p "$TMPDIR"
    tar xf "$ARCHIVE" -C "$TMPDIR" --strip-components=1

    # Copy the needed files into limine/
    for f in "${LIMINE_FILES[@]}" "limine"; do
        if [ -f "$TMPDIR/$f" ]; then
            cp "$TMPDIR/$f" "$LIMINE_DIR/"
        fi
    done
    chmod +x "$LIMINE_BIN"
    rm -rf "$TMPDIR" "$ARCHIVE"
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
"$LIMINE_BIN" bios-install orchid.iso

echo "=== Booting Orchid ==="
qemu-system-x86_64 \
    -cdrom orchid.iso \
    -serial stdio \
    -m 1G \
    -no-reboot \
    -no-shutdown