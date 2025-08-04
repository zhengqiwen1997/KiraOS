#!/bin/bash
# Convenience wrapper for creating FAT32 disk images

echo "🚀 KiraOS Disk Creator"
echo "====================="
echo ""

# Check if tools exist
if [ ! -f "tools/disk/create_fat32_disk_universal.sh" ]; then
    echo "❌ Error: Disk tools not found in tools/disk/"
    echo "Make sure you're running this from the KiraOS root directory"
    exit 1
fi

echo "Creating FAT32 disk image..."
./tools/disk/create_fat32_disk_universal.sh

if [ $? -eq 0 ]; then
    echo ""
    echo "Adding test files..."
    ./tools/disk/add_test_files_universal.sh
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "🎉 FAT32 disk created successfully!"
        echo ""
        echo "🚀 Test with KiraOS:"
        echo "  make && qemu-system-i386 -m 256M -kernel cmake-build-elf/kernel.elf \\"
        echo "    -drive file=test_fat32.img,format=raw,index=0,media=disk"
        echo ""
        echo "💡 In KiraOS shell, try: ls, cat HELLO.TXT, cd BOOT, etc."
    else
        echo "❌ Failed to add test files"
        exit 1
    fi
else
    echo "❌ Failed to create disk image"
    exit 1
fi