# KiraOS Makefile
# Build system for KiraOS bootloader and C++ kernel

# Configuration
ASM = nasm
ASM_FLAGS = -f bin
DD = dd
QEMU = qemu-system-i386

# Directories
BUILD_DIR = build
BOOT_DIR = kernel/boot
KERNEL_DIR = kernel/core
CMAKE_BUILD_DIR = cmake-build

# Files
STAGE1_SRC = $(BOOT_DIR)/stage1.asm
STAGE2_SRC = $(BOOT_DIR)/stage2.asm
STAGE1_BIN = $(BUILD_DIR)/stage1.bin
STAGE2_BIN = $(BUILD_DIR)/stage2.bin
CPP_KERNEL_BIN = $(CMAKE_BUILD_DIR)/kernel.bin
DISK_IMAGE = $(BUILD_DIR)/kiraos.img
SERIAL_LOG = $(BUILD_DIR)/serial.log

# QEMU common flags
QEMU_FLAGS = -drive file=$(DISK_IMAGE),format=raw,if=ide,index=0,media=disk \
             -cpu max \
             -smp 1 \
             -m 256M \
             -no-reboot \
             -no-shutdown \
             -serial file:$(SERIAL_LOG)

#=============================================================================
# Build Targets
#=============================================================================

# Default target
all: $(DISK_IMAGE)

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile stage1 bootloader
$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILD_DIR)
	@echo "Compiling stage1 bootloader..."
	@$(ASM) $(ASM_FLAGS) $< -o $@
	@SIZE=$$(stat -f%z $@); \
	if [ $$SIZE -gt 512 ]; then \
		echo "Error: stage1.bin exceeds 512 bytes ($$SIZE bytes)"; \
		exit 1; \
	fi
	@echo "  ✓ stage1.bin: $$(stat -f%z $@) bytes"

# Compile stage2 bootloader
$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILD_DIR)
	@echo "Compiling stage2 bootloader..."
	@$(ASM) $(ASM_FLAGS) $< -o $@
	@SIZE=$$(stat -f%z $@); \
	if [ $$SIZE -gt 16384 ]; then \
		echo "Error: stage2.bin exceeds 16KB ($$SIZE bytes)"; \
		exit 1; \
	fi
	@echo "  ✓ stage2.bin: $$(stat -f%z $@) bytes"

# Build C++ kernel using CMake
$(CPP_KERNEL_BIN):
	@echo "Building C++ kernel..."
	@mkdir -p $(CMAKE_BUILD_DIR)
	@cd $(CMAKE_BUILD_DIR) && \
		cmake .. -DCMAKE_TOOLCHAIN_FILE=../i686-elf-toolchain.cmake && \
		cmake --build .
	@echo "  ✓ C++ kernel built: $(CPP_KERNEL_BIN)"

# Create bootable disk image
$(DISK_IMAGE): $(STAGE1_BIN) $(STAGE2_BIN) $(CPP_KERNEL_BIN)
	@echo "Creating bootable disk image..."
	@$(DD) if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@$(DD) if=$(STAGE1_BIN) of=$@ conv=notrunc 2>/dev/null
	@echo "  ✓ Stage1 loaded at sector 0 (MBR)"
	@$(DD) if=$(STAGE2_BIN) of=$@ seek=1 conv=notrunc 2>/dev/null
	@echo "  ✓ Stage2 loaded at sector 1"
	@$(DD) if=$(CPP_KERNEL_BIN) of=$@ seek=8 conv=notrunc 2>/dev/null
	@echo "  ✓ C++ kernel loaded at disk sector 8, will be loaded to 0x4000"
	@echo "  ✓ Disk image created: $@"

#=============================================================================
# Run Targets
#=============================================================================

# Run KiraOS in QEMU
run: $(DISK_IMAGE)
	@echo "Starting KiraOS in QEMU..."
	@$(QEMU) $(QEMU_FLAGS) -monitor stdio

# Run with graphics disabled (serial output only)
run-headless: $(DISK_IMAGE)
	@echo "Starting KiraOS in headless mode..."
	@$(QEMU) $(QEMU_FLAGS) -nographic

# Run in debug mode with GDB server
debug: $(DISK_IMAGE)
	@echo "Starting KiraOS in debug mode..."
	@echo "Connect GDB with: target remote localhost:1234"
	@$(QEMU) $(QEMU_FLAGS) -monitor stdio -s -S

#=============================================================================
# Utility Targets
#=============================================================================

# Show serial output
log:
	@if [ -f $(SERIAL_LOG) ]; then \
		echo "=== Serial Output ===" && \
		cat $(SERIAL_LOG); \
	else \
		echo "No serial log found. Run 'make run' first."; \
	fi

# Clean all build files
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR) $(CMAKE_BUILD_DIR)
	@echo "  ✓ Build files cleaned"

# Show help information
help:
	@echo "KiraOS Build System"
	@echo "=================="
	@echo ""
	@echo "Build targets:"
	@echo "  all          - Build bootable disk image (default)"
	@echo "  clean        - Remove all build files"
	@echo ""
	@echo "Run targets:"
	@echo "  run          - Run KiraOS in QEMU with monitor"
	@echo "  run-headless - Run KiraOS without graphics"
	@echo "  debug        - Run with GDB server (port 1234)"
	@echo ""
	@echo "Utility targets:"
	@echo "  log          - Show serial port output"
	@echo "  help         - Show this help message"

.PHONY: all run run-headless debug log clean help 