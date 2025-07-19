#!/bin/bash

set -e

echo "=== KiraOS ATA Driver Test ==="

# Create test disk if it doesn't exist
if [ ! -f test_disk.img ]; then
    echo "Creating test disk image..."
    ./create_test_disk.sh
fi

# Build KiraOS
echo "Building KiraOS..."
cd ../..
make clean > /dev/null 2>&1
make > /dev/null 2>&1
cd kernel/test

echo "Choose test method:"
echo "1) IMG boot method (custom bootloader)"
echo "2) ELF boot method (QEMU -kernel)"
read -p "Enter choice (1 or 2): " choice

case $choice in
    1)
        echo "Testing IMG boot method with ATA driver..."
        echo "QEMU will start with test disk attached as IDE drive"
        echo "Watch for ATA driver test results in the console"
        echo "Press Ctrl+C to exit QEMU"
        echo ""
        
        qemu-system-i386 \
            -m 32M \
            -drive file=../../build/kiraos.img,format=raw,if=floppy \
            -drive file=test_disk.img,format=raw,if=ide \
            -serial stdio \
            -no-reboot \
            -no-shutdown
        ;;
    2)
        echo "Testing ELF boot method with ATA driver..."
        echo "QEMU will start with test disk attached as IDE drive"
        echo "Watch for ATA driver test results in the console"
        echo "Press Ctrl+C to exit QEMU"
        echo ""
        
        qemu-system-i386 \
            -m 32M \
            -kernel ../../cmake-build/kernel.bin \
            -drive file=test_disk.img,format=raw,if=ide \
            -serial stdio \
            -no-reboot \
            -no-shutdown
        ;;
    *)
        echo "Invalid choice. Please run again and choose 1 or 2."
        exit 1
        ;;
esac 