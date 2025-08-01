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
                 -drive file=../$(BUILD_DIR)/test_disk.img,format=raw,if=ide,index=0,media=disk

# QEMU flags for simple ELF kernel boot (no serial logging)
QEMU_ELF_SIMPLE_FLAGS = -kernel kernel.elf \
                        -cpu max \
                        -smp 1 \
                        -m 256M \
                        -no-reboot \
                        -no-shutdown \
                        -drive file=../$(BUILD_DIR)/test_disk.img,format=raw,if=ide,index=0,media=disk

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

# Create test disk image for ELF kernel runs
$(BUILD_DIR)/test_disk.img: | $(BUILD_DIR)
	@echo "Creating test disk image..."
	@$(DD) if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@echo "  ✓ Test disk image created: $@"

# Run KiraOS using ELF kernel (simple and reliable)
run: $(CPP_KERNEL_ELF) $(BUILD_DIR)/test_disk.img
	@echo "Starting KiraOS..."
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_SIMPLE_FLAGS)

# Run KiraOS with serial logging and monitor
run-with-log: $(CPP_KERNEL_ELF) $(BUILD_DIR)/test_disk.img | $(BUILD_DIR)
	@echo "Starting KiraOS with serial logging..."
	@echo "Note: Serial output will be saved to build/serial.log"
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_FLAGS) -monitor stdio

# Run KiraOS simply (no serial logging) - kept for compatibility
run-simple: $(CPP_KERNEL_ELF) $(BUILD_DIR)/test_disk.img
	@echo "Starting KiraOS (simple mode)..."
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_SIMPLE_FLAGS)

# Run with graphics disabled (serial output only)
run-headless: $(CPP_KERNEL_ELF) $(BUILD_DIR)/test_disk.img | $(BUILD_DIR)
	@echo "Starting KiraOS in headless mode..."
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_FLAGS) -nographic

# Run in debug mode with GDB server
debug: $(CPP_KERNEL_ELF) $(BUILD_DIR)/test_disk.img | $(BUILD_DIR)
	@echo "Starting KiraOS in debug mode..."
	@echo "Connect GDB with: target remote localhost:1234"
	@cd $(CMAKE_BUILD_DIR)-elf && $(QEMU) $(QEMU_ELF_FLAGS) -monitor stdio -s -S

# Create bootable disk image without running it
disk: clean-disk $(DISK_IMAGE)
	@echo "Bootable disk image created: $(DISK_IMAGE)"

# Run using legacy disk image method
run-disk: disk
	@echo "Starting KiraOS using disk image (legacy method)..."
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
	@echo "  disk         - Create bootable disk image"
	@echo "  clean        - Remove all build files"
	@echo ""
	@echo "Run targets:"
	@echo "  run          - Run KiraOS (simple, no logging)"
	@echo "  run-with-log - Run KiraOS with serial logging"
	@echo "  run-simple   - Run KiraOS simply (alias for run)"
	@echo "  run-headless - Run KiraOS without graphics"
	@echo "  run-disk     - Run KiraOS using disk image (legacy)"
	@echo "  debug        - Run with GDB server (port 1234)"
	@echo ""
	@echo "Utility targets:"
	@echo "  log          - Show serial port output"
	@echo "  help         - Show this help message"

.PHONY: all disk run run-with-log run-simple run-headless run-disk debug log clean help 