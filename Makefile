# KiraOS Makefile

# Configuration
ASM = nasm
ASM_FLAGS = -f bin
DD = dd
QEMU = qemu-system-i386

# Directories
BUILD_DIR = build
BOOT_DIR = kernel/boot

# Files
STAGE1_SRC = $(BOOT_DIR)/stage1.asm
STAGE2_SRC = $(BOOT_DIR)/stage2.asm
STAGE1_BIN = $(BUILD_DIR)/stage1.bin
STAGE2_BIN = $(BUILD_DIR)/stage2.bin
DISK_IMAGE = $(BUILD_DIR)/kiraos.img
SERIAL_LOG = $(BUILD_DIR)/serial.log

# Default target
all: $(DISK_IMAGE)

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile stage1
$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILD_DIR)
	@echo "Compiling stage1..."
	@$(ASM) $(ASM_FLAGS) $< -o $@
	@SIZE=$$(stat -f%z $@); \
	if [ $$SIZE -gt 512 ]; then \
		echo "Error: stage1.bin is larger than 512 bytes ($$SIZE bytes)"; \
		exit 1; \
	fi
	@echo "stage1.bin: $$(stat -f%z $@) bytes"

# Compile stage2
$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILD_DIR)
	@echo "Compiling stage2..."
	@$(ASM) $(ASM_FLAGS) $< -o $@
	@SIZE=$$(stat -f%z $@); \
	if [ $$SIZE -gt 16384 ]; then \
		echo "Error: stage2.bin is larger than 16KB ($$SIZE bytes)"; \
		exit 1; \
	fi
	@echo "stage2.bin: $$(stat -f%z $@) bytes"

# Create disk image
$(DISK_IMAGE): $(STAGE1_BIN) $(STAGE2_BIN)
	@echo "Creating disk image..."
	@$(DD) if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@$(DD) if=$(STAGE1_BIN) of=$@ conv=notrunc 2>/dev/null
	@$(DD) if=$(STAGE2_BIN) of=$@ seek=1 conv=notrunc 2>/dev/null
	@echo "Disk image created: $@"

# Run QEMU
run: $(DISK_IMAGE)
	@echo "Running QEMU..."
	@$(QEMU) \
		-drive file=$(DISK_IMAGE),format=raw,if=ide,index=0,media=disk \
		-cpu max \
		-smp 2 \
		-m 128M \
		-monitor stdio \
		-d int,cpu_reset \
		-no-reboot \
		-no-shutdown \
		-serial file:$(SERIAL_LOG)

# Run QEMU with debug options
debug: $(DISK_IMAGE)
	@echo "Running QEMU in debug mode..."
	@$(QEMU) \
		-drive file=$(DISK_IMAGE),format=raw,if=ide,index=0,media=disk \
		-cpu max \
		-smp 1 \
		-m 128M \
		-monitor stdio \
		-d int,cpu_reset,guest_errors \
		-no-reboot \
		-no-shutdown \
		-serial file:$(SERIAL_LOG) \
		-s -S

# Clean build files
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

# Help target
help:
	@echo "KiraOS Makefile"
	@echo "Available targets:"
	@echo "  all      - Build everything (default)"
	@echo "  run      - Build and run in QEMU"
	@echo "  debug    - Build and run in QEMU with GDB server enabled"
	@echo "  clean    - Remove build files"
	@echo "  help     - Show this help message"

.PHONY: all run debug clean help 