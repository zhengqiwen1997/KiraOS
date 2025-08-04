#!/bin/bash
# Universal FAT32 disk creation script - detects OS and uses appropriate tools

IMG_FILE="test_fat32.img"
IMG_SIZE_MB=100

echo "🚀 Universal FAT32 Disk Creator for KiraOS"
echo "=========================================="

# Detect operating system
OS=$(uname -s)
echo "Detected OS: $OS"

case $OS in
    "Darwin")
        echo "Using macOS tools (hdiutil + newfs_msdos)..."
        
        # Create disk image
        dd if=/dev/zero of=$IMG_FILE bs=${IMG_SIZE_MB}m count=1
        
        # Format using macOS tools
        DISK_ID=$(hdiutil attach -nomount $IMG_FILE | awk '/disk/ {print $1}')
        if [ -z "$DISK_ID" ]; then
            echo "❌ Error: Failed to attach disk image"
            exit 1
        fi
        
        newfs_msdos -F 32 -v NO_NAME $DISK_ID
        hdiutil detach $DISK_ID
        echo "✅ macOS FAT32 creation successful!"
        ;;
        
    "Linux")
        echo "Using Linux tools (mkfs.fat)..."
        
        # Check if mkfs.fat is available
        if ! command -v mkfs.fat &> /dev/null; then
            echo "❌ Error: mkfs.fat not found"
            echo "Install it with: sudo apt install dosfstools (Ubuntu/Debian)"
            echo "                 sudo yum install dosfstools (RHEL/CentOS)"
            echo "                 sudo pacman -S dosfstools (Arch)"
            exit 1
        fi
        
        # Create and format disk image
        dd if=/dev/zero of=$IMG_FILE bs=1M count=$IMG_SIZE_MB
        mkfs.fat -F 32 -n NO_NAME $IMG_FILE
        echo "✅ Linux FAT32 creation successful!"
        ;;
        
    "CYGWIN"*|"MINGW"*|"MSYS"*)
        echo "Windows environment detected..."
        echo "⚠️  Windows FAT32 creation requires additional setup"
        echo "Recommended approaches:"
        echo "1. Use WSL (Windows Subsystem for Linux) with Linux tools"
        echo "2. Use the PowerShell script: create_fat32_disk.ps1"
        echo "3. Use a Linux VM"
        
        # Create raw image only
        dd if=/dev/zero of=$IMG_FILE bs=1M count=$IMG_SIZE_MB
        echo "✅ Raw image created, manual formatting required"
        ;;
        
    *)
        echo "❌ Unsupported OS: $OS"
        echo "Supported platforms:"
        echo "  - macOS (Darwin) ✅"
        echo "  - Linux ✅" 
        echo "  - Windows (with WSL/Cygwin) ⚠️"
        exit 1
        ;;
esac

# Show result
echo ""
echo "📊 Image Information:"
echo "  File: $IMG_FILE"
echo "  Size: $(du -h $IMG_FILE | awk '{print $1}')"

# Verify FAT table if possible
if [ "$OS" = "Darwin" ] || [ "$OS" = "Linux" ]; then
    echo ""
    echo "🔍 FAT Table Verification:"
    FAT_OFFSET=$((32 * 512))
    echo "First FAT sector (sector 32):"
    hexdump -C -s $FAT_OFFSET -n 32 $IMG_FILE | head -2
fi

echo ""
echo "🎉 FAT32 disk image ready for KiraOS!"