#!/bin/bash
# Universal test file addition script - detects OS and uses appropriate tools

IMG_FILE="test_fat32.img"
MOUNT_POINT="/tmp/kira_fat32_mount"

echo "🚀 Universal Test File Adder for KiraOS"
echo "========================================"

# Check if image exists
if [ ! -f "$IMG_FILE" ]; then
    echo "❌ Error: $IMG_FILE not found"
    echo "Run create_fat32_disk_universal.sh first"
    exit 1
fi

# Detect operating system
OS=$(uname -s)
echo "Detected OS: $OS"

# Function to create test files (common content)
create_test_files() {
    local mount_point=$1
    local use_sudo=$2
    
    echo "Creating test files..."
    
    # Helper function for file creation
    create_file() {
        local file_path=$1
        local content=$2
        if [ "$use_sudo" = "true" ]; then
            sudo bash -c "echo '$content' > '$file_path'"
        else
            echo "$content" > "$file_path"
        fi
    }
    
    append_file() {
        local file_path=$1
        local content=$2
        if [ "$use_sudo" = "true" ]; then
            sudo bash -c "echo '$content' >> '$file_path'"
        else
            echo "$content" >> "$file_path"
        fi
    }
    
    make_dir() {
        local dir_path=$1
        if [ "$use_sudo" = "true" ]; then
            sudo mkdir "$dir_path"
        else
            mkdir "$dir_path"
        fi
    }
    
    # Create test files
    create_file "$mount_point/HELLO.TXT" "Hello from KiraOS!"
    append_file "$mount_point/HELLO.TXT" "This is a test file for the FAT32 filesystem."
    
    # Create README
    if [ "$use_sudo" = "true" ]; then
        sudo bash -c "cat > '$mount_point/README.TXT' << 'EOF'
KiraOS FAT32 Test Filesystem
============================

This directory contains test files for KiraOS.

Files:
- HELLO.TXT: Simple greeting file
- README.TXT: This file
- BOOT: Directory for boot files
- CONFIG.SYS: System configuration

Enjoy exploring your filesystem!
EOF"
    else
        cat > "$mount_point/README.TXT" << 'EOF'
KiraOS FAT32 Test Filesystem
============================

This directory contains test files for KiraOS.

Files:
- HELLO.TXT: Simple greeting file
- README.TXT: This file
- BOOT: Directory for boot files
- CONFIG.SYS: System configuration

Enjoy exploring your filesystem!
EOF
    fi
    
    # Create directories and files
    make_dir "$mount_point/BOOT"
    create_file "$mount_point/BOOT/BOOTINFO.TXT" "Boot configuration files go here"
    
    create_file "$mount_point/CONFIG.SYS" "# KiraOS Configuration"
    append_file "$mount_point/CONFIG.SYS" "DEVICE=ATA0"
    append_file "$mount_point/CONFIG.SYS" "FILESYSTEM=FAT32"
    
    make_dir "$mount_point/APPS"
    create_file "$mount_point/APPS/README.TXT" "Application files stored here"
    create_file "$mount_point/APPS/TEST.SH" "#!/bin/sh"
    append_file "$mount_point/APPS/TEST.SH" "echo 'Hello from shell script!'"

    # Install /bin and copy staged ls ELF if present
    make_dir "$mount_point/bin"
    # Prefer cmake-build-disk/bin, fallback to cmake-build-elf/bin
    BIN_SRC_BASE="${PWD}/cmake-build-disk/bin"
    if [ ! -f "$BIN_SRC_BASE/ls" ]; then BIN_SRC_BASE="${PWD}/cmake-build-elf/bin"; fi
    if [ -f "$BIN_SRC_BASE/ls" ]; then
        if [ "$use_sudo" = "true" ]; then
            sudo cp "$BIN_SRC_BASE/ls" "$mount_point/bin/ls"
        else
            cp "$BIN_SRC_BASE/ls" "$mount_point/bin/ls"
        fi
        echo "Installed /bin/ls"
    else
        echo "Note: /bin/ls not found in $BIN_SRC_BASE; build ls_user.elf first"
    fi

    for prog in cat mkdir rmdir; do
        if [ -f "$BIN_SRC_BASE/$prog" ]; then
            if [ "$use_sudo" = "true" ]; then
                sudo cp "$BIN_SRC_BASE/$prog" "$mount_point/bin/$prog"
            else
                cp "$BIN_SRC_BASE/$prog" "$mount_point/bin/$prog"
            fi
            echo "Installed /bin/$prog"
        else
            echo "Note: $prog not found in $BIN_SRC_BASE; build ${prog}_user.elf first"
        fi
    done
}

case $OS in
    "Darwin")
        echo "Using macOS tools (hdiutil)..."
        
        # Create mount point
        mkdir -p "$MOUNT_POINT"
        
        # Mount using hdiutil
        DISK=$(hdiutil attach -mountpoint "$MOUNT_POINT" $IMG_FILE | grep "/dev/disk" | cut -d' ' -f1)
        if [ -z "$DISK" ]; then
            echo "❌ Error: Failed to mount image"
            exit 1
        fi
        
        echo "Mounted at: $MOUNT_POINT (disk: $DISK)"
        
        # Create files (no sudo needed on macOS with hdiutil)
        create_test_files "$MOUNT_POINT" false
        
        # Show contents
        echo "Directory contents:"
        ls -la "$MOUNT_POINT" | grep -v "^total\|^\.$"
        
        # Unmount
        hdiutil detach "$DISK"
        echo "✅ macOS file addition successful!"
        ;;
        
    "Linux")
        echo "Using Linux tools (loopback + mount)..."
        
        # Check if running as root or if sudo is available
        if [ "$EUID" -ne 0 ] && ! command -v sudo &> /dev/null; then
            echo "❌ Error: Root access or sudo required for mounting"
            exit 1
        fi
        
        # Create mount point
        if [ "$EUID" -eq 0 ]; then
            mkdir -p "$MOUNT_POINT" 
            USE_SUDO=false
        else
            sudo mkdir -p "$MOUNT_POINT"
            USE_SUDO=true
        fi
        
        # Set up loopback device
        if [ "$USE_SUDO" = "true" ]; then
            LOOP_DEVICE=$(sudo losetup --find --show $IMG_FILE)
            sudo mount $LOOP_DEVICE "$MOUNT_POINT"
        else
            LOOP_DEVICE=$(losetup --find --show $IMG_FILE)
            mount $LOOP_DEVICE "$MOUNT_POINT"
        fi
        
        echo "Loopback device: $LOOP_DEVICE"
        echo "Mounted at: $MOUNT_POINT"
        
        # Create files
        create_test_files "$MOUNT_POINT" $USE_SUDO
        
        # Show contents
        echo "Directory contents:"
        ls -la "$MOUNT_POINT" | grep -v "^total\|^\.$"
        
        # Cleanup
        if [ "$USE_SUDO" = "true" ]; then
            sudo umount "$MOUNT_POINT"
            sudo losetup -d $LOOP_DEVICE
            sudo rmdir "$MOUNT_POINT"
        else
            umount "$MOUNT_POINT"
            losetup -d $LOOP_DEVICE
            rmdir "$MOUNT_POINT"
        fi
        
        echo "✅ Linux file addition successful!"
        ;;
        
    "CYGWIN"*|"MINGW"*|"MSYS"*)
        echo "Windows environment detected..."
        echo "⚠️  Automatic file addition not supported on Windows"
        echo "Recommended approaches:"
        echo "1. Use WSL with Linux tools"
        echo "2. Manually mount the image in Windows and copy files"
        echo "3. Use a Linux VM"
        exit 1
        ;;
        
    *)
        echo "❌ Unsupported OS: $OS"
        exit 1
        ;;
esac

echo ""
echo "🎉 Test files added successfully!"
echo ""
echo "📁 Files added:"
echo "  - HELLO.TXT (greeting file)"
echo "  - README.TXT (documentation)"  
echo "  - CONFIG.SYS (system config)"
echo "  - BOOT/ (directory with BOOTINFO.TXT)"
echo "  - APPS/ (directory with README.TXT and TEST.SH)"