#!/bin/bash

echo "Adding test files to FAT32 image..."

# Create a temporary mount point
MOUNT_POINT="/tmp/kira_fat32_mount"
mkdir -p "$MOUNT_POINT"

# Attach the disk image
echo "Attaching FAT32 image..."
DISK=$(hdiutil attach -mountpoint "$MOUNT_POINT" test_fat32.img | grep "/dev/disk" | cut -d' ' -f1)
echo "Mounted at: $MOUNT_POINT (disk: $DISK)"

# Add some test files
echo "Creating test files..."

# Create a simple text file
echo "Hello from KiraOS!" > "$MOUNT_POINT/HELLO.TXT"
echo "This is a test file for the FAT32 filesystem." >> "$MOUNT_POINT/HELLO.TXT"

# Create a README file
cat > "$MOUNT_POINT/README.TXT" << 'EOF'
KiraOS FAT32 Test Filesystem
============================

This directory contains test files for KiraOS.

Files:
- HELLO.TXT: Simple greeting file
- README.TXT: This file
- BOOT: Directory for boot files
- CONFIG.SYS: System configuration (empty)

Enjoy exploring your filesystem!
EOF

# Create a directory
mkdir "$MOUNT_POINT/BOOT"
echo "Boot configuration files go here" > "$MOUNT_POINT/BOOT/BOOTINFO.TXT"

# Create a small config file
echo "# KiraOS Configuration" > "$MOUNT_POINT/CONFIG.SYS"
echo "DEVICE=ATA0" >> "$MOUNT_POINT/CONFIG.SYS"
echo "FILESYSTEM=FAT32" >> "$MOUNT_POINT/CONFIG.SYS"

# Create another directory with files
mkdir "$MOUNT_POINT/APPS"
echo "Application files stored here" > "$MOUNT_POINT/APPS/README.TXT"
echo "#!/bin/sh" > "$MOUNT_POINT/APPS/TEST.SH"
echo "echo 'Hello from shell script!'" >> "$MOUNT_POINT/APPS/TEST.SH"

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

# Unmount the disk
echo ""
echo "Unmounting disk..."
hdiutil detach "$DISK"

echo "✅ Test files added successfully to test_fat32.img!"
echo ""
echo "Files added:"
echo "  - HELLO.TXT (greeting file)"
echo "  - README.TXT (documentation)"
echo "  - CONFIG.SYS (system config)"
echo "  - BOOT/ (directory with BOOTINFO.TXT)"
echo "  - APPS/ (directory with README.TXT and TEST.SH)"