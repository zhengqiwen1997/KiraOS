#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build

# Compile stage1
echo "Compiling stage1..."
nasm -f bin kernel/boot/stage1.asm -o build/stage1.bin
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile stage1"
    exit 1
fi

# Check stage1 size (must fit in MBR - 512 bytes)
STAGE1_SIZE=$(stat -f%z build/stage1.bin)
if [ $STAGE1_SIZE -gt 512 ]; then
    echo "Error: stage1.bin is larger than 512 bytes ($STAGE1_SIZE bytes)"
    exit 1
fi

# Compile stage2
echo "Compiling stage2..."
nasm -f bin kernel/boot/stage2.asm -o build/stage2.bin
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile stage2"
    exit 1
fi

# Check stage2 size (allow up to 16KB)
STAGE2_SIZE=$(stat -f%z build/stage2.bin)
if [ $STAGE2_SIZE -gt 16384 ]; then
    echo "Error: stage2.bin is larger than 16KB ($STAGE2_SIZE bytes)"
    exit 1
fi

# Calculate number of sectors needed for stage2
SECTORS=$(( ($STAGE2_SIZE + 511) / 512 ))
echo "Stage2 size: $STAGE2_SIZE bytes ($SECTORS sectors)"

# Create disk image
echo "Creating disk image..."
# Create a 1.44MB floppy image (2880 sectors * 512 bytes)
dd if=/dev/zero of=build/kiraos.img bs=512 count=2880 2>/dev/null

# Write MBR (stage1) to first sector
echo "Writing stage1 to MBR..."
dd if=build/stage1.bin of=build/kiraos.img conv=notrunc 2>/dev/null

# Write stage2 starting at sector 1
echo "Writing stage2 ($SECTORS sectors)..."
dd if=build/stage2.bin of=build/kiraos.img seek=1 conv=notrunc 2>/dev/null

echo "Build complete. Running QEMU..."

# Run QEMU with optimized settings
qemu-system-i386 \
    -drive file=build/kiraos.img,format=raw,if=ide,index=0,media=disk \
    -cpu max \
    -smp 2 \
    -m 128M \
    -monitor stdio \
    -d int \
    -no-reboot \
    -no-shutdown 