#!/bin/bash

# KiraOS Build Script
# This script compiles the bootloader and creates a disk image

# ===== Configuration =====
STAGE1_SRC="kernel/boot/stage1.asm"
STAGE2_SRC="kernel/boot/stage2.asm"
BUILD_DIR="build"
IMAGE_NAME="kiraos.img"
STAGE1_MAX_SIZE=512  # MBR must be 512 bytes
STAGE2_MAX_SIZE=16384  # 16KB (32 sectors)

# ===== Functions =====

# Print colored messages
print_info() {
    echo -e "\033[1;34m[INFO]\033[0m $1"
}

print_success() {
    echo -e "\033[1;32m[SUCCESS]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1" >&2
}

# Clean build directory
clean_build() {
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
}

# Compile an assembly file
compile_asm() {
    local src="$1"
    local out="$2"
    local max_size="$3"
    local name="$(basename "$src" .asm)"
    
    print_info "Compiling $name..."
    nasm -f bin "$src" -o "$out"
    
    if [ $? -ne 0 ]; then
        print_error "Failed to compile $name"
        return 1
    fi
    
    # Check file size
    local size=$(stat -f%z "$out")
    if [ $size -gt $max_size ]; then
        print_error "$name is too large: $size bytes (max: $max_size bytes)"
        return 1
    fi
    
    print_success "$name compiled: $size bytes"
    return 0
}

# Create disk image
create_disk_image() {
    print_info "Creating disk image..."
    
    # Create a 1.44MB floppy image (2880 sectors * 512 bytes)
    dd if=/dev/zero of="$BUILD_DIR/$IMAGE_NAME" bs=512 count=2880 2>/dev/null
    
    # Write MBR (stage1) to first sector
    print_info "Writing stage1 to MBR..."
    dd if="$BUILD_DIR/stage1.bin" of="$BUILD_DIR/$IMAGE_NAME" conv=notrunc 2>/dev/null
    
    # Calculate number of sectors needed for stage2
    local stage2_size=$(stat -f%z "$BUILD_DIR/stage2.bin")
    local sectors=$(( ($stage2_size + 511) / 512 ))
    
    # Write stage2 starting at sector 1
    print_info "Writing stage2 ($sectors sectors)..."
    dd if="$BUILD_DIR/stage2.bin" of="$BUILD_DIR/$IMAGE_NAME" seek=1 conv=notrunc 2>/dev/null
    
    print_success "Disk image created: $BUILD_DIR/$IMAGE_NAME"
}

# Run QEMU
run_qemu() {
    print_info "Running QEMU..."
    qemu-system-i386 \
        -drive file="$BUILD_DIR/$IMAGE_NAME",format=raw,if=ide,index=0,media=disk \
        -cpu max \
        -smp 2 \
        -m 128M \
        -monitor stdio \
        -d int,cpu_reset \
        -no-reboot \
        -no-shutdown \
        -serial file:"$BUILD_DIR/serial.log"
}

# Show help
show_help() {
    echo "KiraOS Build Script"
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -c, --clean    Clean build directory before building"
    echo "  -b, --build    Build only, don't run QEMU"
    echo "  -r, --run      Run QEMU with existing image (don't rebuild)"
    echo ""
}

# ===== Main =====

# Parse command line arguments
CLEAN=0
BUILD_ONLY=0
RUN_ONLY=0

for arg in "$@"; do
    case $arg in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN=1
            ;;
        -b|--build)
            BUILD_ONLY=1
            ;;
        -r|--run)
            RUN_ONLY=1
            ;;
        *)
            print_error "Unknown option: $arg"
            show_help
            exit 1
            ;;
    esac
done

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Clean if requested
if [ $CLEAN -eq 1 ]; then
    clean_build
fi

# Run only
if [ $RUN_ONLY -eq 1 ]; then
    if [ ! -f "$BUILD_DIR/$IMAGE_NAME" ]; then
        print_error "Disk image not found: $BUILD_DIR/$IMAGE_NAME"
        exit 1
    fi
    run_qemu
    exit 0
fi

# Build
if [ $RUN_ONLY -eq 0 ]; then
    # Compile stage1
    compile_asm "$STAGE1_SRC" "$BUILD_DIR/stage1.bin" $STAGE1_MAX_SIZE
    if [ $? -ne 0 ]; then
        exit 1
    fi

    # Compile stage2
    compile_asm "$STAGE2_SRC" "$BUILD_DIR/stage2.bin" $STAGE2_MAX_SIZE
    if [ $? -ne 0 ]; then
        exit 1
    fi

    # Create disk image
    create_disk_image
fi

# Run QEMU if not build-only
if [ $BUILD_ONLY -eq 0 ]; then
    run_qemu
fi

exit 0 