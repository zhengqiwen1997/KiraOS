#!/bin/bash

# Create a proper FAT32 disk image for KiraOS
echo "Creating FAT32 disk image..."

# Remove old image
rm -f test_fat32.img

# Create 100MB image file
dd if=/dev/zero of=test_fat32.img bs=1M count=100

# Use diskutil on macOS to create FAT32 filesystem
echo "Formatting as FAT32..."

# Attach as disk image
DISK=$(hdiutil attach -nomount test_fat32.img | cut -d' ' -f1)
echo "Attached disk: $DISK"

# Format as FAT32
newfs_msdos -F 32 $DISK

# Detach the disk
hdiutil detach $DISK

echo "FAT32 disk image created successfully!"
echo "Image size: $(ls -lh test_fat32.img | awk '{print $5}')"

# Verify the FAT table structure
echo ""
echo "Checking FAT table initialization:"
echo "First FAT sector (sector 32):"
dd if=test_fat32.img bs=512 skip=32 count=1 2>/dev/null | hexdump -C | head -5