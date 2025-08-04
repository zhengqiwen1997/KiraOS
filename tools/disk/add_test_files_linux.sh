#!/bin/bash
# Linux version of adding test files to FAT32 image

echo "Adding test files to FAT32 image (Linux)..."

IMG_FILE="test_fat32.img"
MOUNT_POINT="/tmp/kira_fat32_mount"

# Create temporary mount point
sudo mkdir -p "$MOUNT_POINT"

# Set up loopback device and mount
echo "Setting up loopback device..."
LOOP_DEVICE=$(sudo losetup --find --show $IMG_FILE)
echo "Loopback device: $LOOP_DEVICE"

echo "Mounting FAT32 image..."
sudo mount $LOOP_DEVICE "$MOUNT_POINT"

# Add some test files
echo "Creating test files..."

# Create a simple text file
sudo bash -c "echo 'Hello from KiraOS!' > '$MOUNT_POINT/HELLO.TXT'"
sudo bash -c "echo 'This is a test file for the FAT32 filesystem.' >> '$MOUNT_POINT/HELLO.TXT'"

# Create a README file
sudo bash -c "cat > '$MOUNT_POINT/README.TXT' << 'EOF'
KiraOS FAT32 Test Filesystem
============================

This directory contains test files for KiraOS.

Files:
- HELLO.TXT: Simple greeting file
- README.TXT: This file
- BOOT: Directory for boot files
- CONFIG.SYS: System configuration (empty)

Enjoy exploring your filesystem!
EOF"

# Create a directory
sudo mkdir "$MOUNT_POINT/BOOT"
sudo bash -c "echo 'Boot configuration files go here' > '$MOUNT_POINT/BOOT/BOOTINFO.TXT'"

# Create a small config file
sudo bash -c "echo '# KiraOS Configuration' > '$MOUNT_POINT/CONFIG.SYS'"
sudo bash -c "echo 'DEVICE=ATA0' >> '$MOUNT_POINT/CONFIG.SYS'"
sudo bash -c "echo 'FILESYSTEM=FAT32' >> '$MOUNT_POINT/CONFIG.SYS'"

# Create another directory with files
sudo mkdir "$MOUNT_POINT/APPS"
sudo bash -c "echo 'Application files stored here' > '$MOUNT_POINT/APPS/README.TXT'"
sudo bash -c "echo '#!/bin/sh' > '$MOUNT_POINT/APPS/TEST.SH'"
sudo bash -c "echo 'echo \"Hello from shell script!\"' >> '$MOUNT_POINT/APPS/TEST.SH'"

echo "Files created successfully!"
echo ""
echo "Directory contents:"
ls -la "$MOUNT_POINT"
echo ""
echo "Boot directory contents:"
ls -la "$MOUNT_POINT/BOOT"
echo ""
echo "Apps directory contents:"
ls -la "$MOUNT_POINT/APPS"

# Unmount and cleanup
echo ""
echo "Unmounting and cleaning up..."
sudo umount "$MOUNT_POINT"
sudo losetup -d $LOOP_DEVICE
sudo rmdir "$MOUNT_POINT"

echo "✅ Test files added successfully to test_fat32.img (Linux)!"
echo ""
echo "Files added:"
echo "  - HELLO.TXT (greeting file)"
echo "  - README.TXT (documentation)"
echo "  - CONFIG.SYS (system config)"
echo "  - BOOT/ (directory with BOOTINFO.TXT)"
echo "  - APPS/ (directory with README.TXT and TEST.SH)"
echo ""
echo "Note: This script requires sudo for mount operations"