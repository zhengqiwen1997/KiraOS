#!/bin/bash
# Linux version of FAT32 disk creation script

IMG_FILE="test_fat32.img"
IMG_SIZE_MB=100

echo "Creating FAT32 disk image (Linux)..."

# Create empty disk image
dd if=/dev/zero of=$IMG_FILE bs=1M count=$IMG_SIZE_MB

echo "Formatting as FAT32..."
# Linux uses mkfs.fat (part of dosfstools package)
# -F 32: FAT32
# -n NO_NAME: Volume label
mkfs.fat -F 32 -n NO_NAME $IMG_FILE

echo "FAT32 disk image created successfully!"
echo "Image size: $(du -h $IMG_FILE | awk '{print $1}')"

echo ""
echo "Checking FAT table initialization:"
# Calculate the offset to the first FAT table (32 reserved sectors * 512 bytes)
FAT_OFFSET=$((32 * 512)) 
echo "First FAT sector (sector 32):"
hexdump -C -s $FAT_OFFSET -n 512 $IMG_FILE

echo ""
echo "✅ Linux FAT32 disk creation complete!"
echo "Note: Install dosfstools if mkfs.fat is missing: sudo apt install dosfstools"