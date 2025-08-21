# KiraOS Makefile
# Build system for KiraOS bootloader and C++ kernel

# Configuration
ASM = nasm
ASM_FLAGS = -f bin
DD = dd
QEMU = qemu-system-i386

# Directories
BUILD_DIR = build
BOOT_DIR = kernel/boot/img
KERNEL_DIR = kernel/core
CMAKE_BUILD_DIR = cmake-build

# Files
STAGE1_SRC = $(BOOT_DIR)/stage1.asm
STAGE2_SRC = $(BOOT_DIR)/stage2.asm
STAGE1_BIN = $(BUILD_DIR)/stage1.bin
STAGE2_BIN = $(BUILD_DIR)/stage2.bin
CPP_KERNEL_BIN = $(CMAKE_BUILD_DIR)-elf/kernel.bin
CPP_KERNEL_ELF = $(CMAKE_BUILD_DIR)-elf/kernel.elf
DISK_IMAGE = $(BUILD_DIR)/kiraos.img
SERIAL_LOG = $(BUILD_DIR)/serial.log

# QEMU flags for disk image boot
QEMU_DISK_FLAGS = -drive file=$(DISK_IMAGE),format=raw,if=ide,index=0,media=disk \
                  -cpu max \
                  -smp 1 \
                  -m 256M \
                  -no-reboot \
                  -no-shutdown \
                  -serial file:$(SERIAL_LOG)

# QEMU flags for ELF kernel boot (recommended)
QEMU_ELF_FLAGS = -kernel kernel.elf \
                 -cpu max \
                 -smp 1 \
                 -m 256M \
                 -no-reboot \
                 -no-shutdown \
                 -serial file:../$(SERIAL_LOG) \
                 -drive file=../test_fat32.img,format=raw,index=0,media=disk

# QEMU flags for simple ELF kernel boot (with FAT32 filesystem and console)
QEMU_ELF_SIMPLE_FLAGS = -kernel kernel.elf \
                        -m 256M \
                        -drive file=../test_fat32.img,format=raw,index=0,media=disk \
                        -serial stdio

#=============================================================================
# Build Targets
#=============================================================================

# Default target
all: $(CPP_KERNEL_ELF)

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
$(CPP_KERNEL_ELF):
	@echo "Building C++ kernel for ELF boot..."
	@mkdir -p $(CMAKE_BUILD_DIR)-elf
	@cd $(CMAKE_BUILD_DIR)-elf && \
		cmake .. -DCMAKE_TOOLCHAIN_FILE=../i686-elf-toolchain.cmake -DDISK_BOOT_ONLY=0 && \
		cmake --build .
	@echo "  ✓ C++ kernel built: $(CPP_KERNEL_ELF)"



# Build kernel for disk boot (minimal, no FAT32 tests)
$(CMAKE_BUILD_DIR)-disk/kernel.elf:
	@echo "Building C++ kernel for disk boot..."
	@mkdir -p $(CMAKE_BUILD_DIR)-disk
	@cd $(CMAKE_BUILD_DIR)-disk && \
		cmake .. -DCMAKE_TOOLCHAIN_FILE=../i686-elf-toolchain.cmake -DDISK_BOOT_ONLY=1 && \
		cmake --build .
	@echo "  ✓ C++ kernel built for disk boot: $@"

# Legacy target for binary kernel
$(CPP_KERNEL_BIN): $(CPP_KERNEL_ELF)
	@echo "Ensuring kernel binary exists..."
	@if [ ! -f $@ ]; then \
		echo "Error: kernel.bin not found. Run 'make' first."; \
		exit 1; \
	fi

# Create bootable disk image (legacy method)
$(DISK_IMAGE): $(STAGE1_BIN) $(STAGE2_BIN) $(CMAKE_BUILD_DIR)-disk/kernel.elf
	@echo "Creating bootable disk image..."
	@$(DD) if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@$(DD) if=$(STAGE1_BIN) of=$@ conv=notrunc 2>/dev/null
	@echo "  ✓ Stage1 loaded at sector 0 (MBR)"
	@$(DD) if=$(STAGE2_BIN) of=$@ seek=1 conv=notrunc 2>/dev/null
	@echo "  ✓ Stage2 loaded at sectors 1-4"
	@$(DD) if=$(CMAKE_BUILD_DIR)-disk/kernel.bin of=$@ seek=5 conv=notrunc 2>/dev/null
	@echo "  ✓ C++ kernel loaded at disk sector 5, will be moved to 0x100000 (1MB)"
	@echo "  ✓ Disk image created: $@"

#=============================================================================
# Run Targets
#=============================================================================

# Create FAT32 disk image with test files for KiraOS
test_fat32.img:
	@echo "Creating FAT32 disk image with test files..."
	@if [ ! -f test_fat32.img ]; then \
		echo "  Running disk creation script..."; \
		./create_disk.sh; \
	else \
		echo "  ✓ FAT32 disk image already exists: test_fat32.img"; \
	fi

# Legacy test disk image (kept for compatibility)
$(BUILD_DIR)/test_disk.img: | $(BUILD_DIR)
	@echo "Creating legacy test disk image..."
	@$(DD) if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@echo "  ✓ Legacy test disk image created: $@"

# Run KiraOS using ELF kernel with FAT32 filesystem
run: $(CPP_KERNEL_ELF) test_fat32.img
	@echo "Starting KiraOS with FAT32 filesystem..."
	@echo "💡 Try these commands in KiraOS shell: ls, cat HELLO.TXT, cd BOOT, help"
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_SIMPLE_FLAGS)

# Run KiraOS with serial logging and monitor
run-with-log: $(CPP_KERNEL_ELF) test_fat32.img | $(BUILD_DIR)
	@echo "Starting KiraOS with FAT32 filesystem and serial logging..."
	@echo "Note: Serial output will be saved to build/serial.log"
	@echo "💡 Try these commands in KiraOS shell: ls, cat HELLO.TXT, cd BOOT, help"
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_FLAGS) -monitor stdio

# Run KiraOS simply (no serial logging) - kept for compatibility
run-simple: $(CPP_KERNEL_ELF) test_fat32.img
	@echo "Starting KiraOS with FAT32 filesystem (simple mode)..."
	@echo "💡 Try these commands in KiraOS shell: ls, cat HELLO.TXT, cd BOOT, help"
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_SIMPLE_FLAGS)

# Run with graphics disabled (serial output only)
run-headless: $(CPP_KERNEL_ELF) test_fat32.img | $(BUILD_DIR)
	@echo "Starting KiraOS with FAT32 filesystem in headless mode..."
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_FLAGS) -nographic

# Run in debug mode with GDB server
debug: $(CPP_KERNEL_ELF) test_fat32.img | $(BUILD_DIR)
	@echo "Starting KiraOS with FAT32 filesystem in debug mode..."
	@echo "Connect GDB with: target remote localhost:1234"
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_FLAGS) -monitor stdio -s -S

# Create bootable disk image without running it
disk: clean-disk $(DISK_IMAGE)
	@echo "Bootable disk image created: $(DISK_IMAGE)"

# Run KiraOS using ELF kernel with FAT32 disk (same as run but with monitor)
run-disk: $(CMAKE_BUILD_DIR)-disk/kernel.elf test_fat32.img | $(BUILD_DIR)
	@echo "Starting KiraOS (disk build) with FAT32 filesystem and monitor console..."
	@echo "💡 Try these commands in KiraOS shell: ls, cat HELLO.TXT, cd BOOT, help"
	@cd $(CMAKE_BUILD_DIR)-disk && $(QEMU) $(QEMU_ELF_FLAGS) -monitor stdio

# Run using legacy custom bootloader method
run-legacy-disk: disk
	@echo "Starting KiraOS using legacy disk image (custom bootloader)..."
	@$(QEMU) $(QEMU_DISK_FLAGS) -monitor stdio

# Clean and recreate disk image
clean-disk:
	@echo "Recreating clean disk image..."
	@rm -f $(DISK_IMAGE)

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
	@rm -rf $(BUILD_DIR) $(CMAKE_BUILD_DIR) $(CMAKE_BUILD_DIR)-elf $(CMAKE_BUILD_DIR)-disk
	@echo "  ✓ Build files cleaned"

# Show help information
help:
	@echo "KiraOS Build System"
	@echo "=================="
	@echo ""
	@echo "Build targets:"
	@echo "  all          - Build ELF kernel (default)"
	@echo "  disk         - Create bootable disk image (legacy)"
	@echo "  test_fat32.img - Create FAT32 disk image with test files"
	@echo "  clean        - Remove all build files"
	@echo ""
	@echo "Run targets (with FAT32 filesystem):"
	@echo "  run          - Run KiraOS with FAT32 disk (recommended)"
	@echo "  run-with-log - Run KiraOS with serial logging"
	@echo "  run-simple   - Run KiraOS simply (alias for run)"
	@echo "  run-headless - Run KiraOS without graphics"
	@echo "  run-disk     - Run KiraOS with FAT32 disk and monitor"
	@echo "  debug        - Run with GDB server (port 1234)"
	@echo ""
	@echo "Legacy targets:"
	@echo "  run-legacy-disk - Run KiraOS using custom bootloader"
	@echo ""
	@echo "🎮 KiraOS Shell Commands:"
	@echo "  ls           - List files and directories"
	@echo "  cat FILE     - Read file contents"
	@echo "  cd DIR       - Change directory"
	@echo "  pwd          - Show current directory"
	@echo "  help         - Show available commands"
	@echo ""
	@echo "Utility targets:"
	@echo "  log          - Show serial port output"
	@echo "  help         - Show this help message"

.PHONY: all disk test_fat32.img run run-with-log run-simple run-headless run-disk run-legacy-disk debug log clean help 