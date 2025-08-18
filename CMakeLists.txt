cmake_minimum_required(VERSION 3.20)

# Cross-compilation settings - must be set before project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Set cross-compiler
set(CMAKE_CXX_COMPILER i686-elf-g++)
set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_ASM_COMPILER i686-elf-gcc)

# Skip compiler tests for cross-compilation
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_C_COMPILER_WORKS 1)

# Disable macOS-specific flags
set(CMAKE_OSX_DEPLOYMENT_TARGET "")
set(CMAKE_OSX_SYSROOT "")
set(CMAKE_OSX_ARCHITECTURES "")

project(KiraOS VERSION 0.1.0 LANGUAGES CXX ASM)

# Handle DISK_BOOT_ONLY flag
if(DISK_BOOT_ONLY)
    add_definitions(-DDISK_BOOT_ONLY)
endif()

# Find objcopy for binary conversion
find_program(CMAKE_OBJCOPY i686-elf-objcopy)
if(NOT CMAKE_OBJCOPY)
  message(FATAL_ERROR "i686-elf-objcopy not found, please install it")
endif()

# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for kernel code
set(KERNEL_CXX_FLAGS
    "-ffreestanding"                  # Standalone environment (no stdlib)
    "-fno-exceptions"                 # No exception handling
    "-fno-rtti"                      # No runtime type information
    "-mno-red-zone"                  # Disable red zone
    "-fno-stack-protector"           # No stack protection
    "-fno-pie"                       # No position independent executable
    "-fno-pic"                       # No position independent code
    "-m32"                           # Force 32-bit mode
    "-nostdlib"                      # No standard library
    "-nostdinc"                      # No standard includes
    "-msse"                          # Enable SSE instructions
    "-mfpmath=sse"                   # Use SSE for floating-point math
)

# Linker flags for kernel
set(KERNEL_LINK_FLAGS
    "-nostdlib"                      # No standard library
    "-m32"                           # Force 32-bit mode
    "-static"                        # Static linking
    "-Wl,-z,noexecstack"             # No executable stack
)

# Create kernel library
add_library(kernel_core STATIC
    kernel/boot/elf/multiboot_entry.s
    kernel/core/boot/kernel_entry.cpp
    kernel/core/kernel.cpp
    kernel/core/process.cpp
    kernel/core/syscalls.cpp
    kernel/core/usermode.cpp
    kernel/core/memory_operators.cpp
    kernel/core/runtime_support.cpp
    kernel/core/sync.cpp
    kernel/core/k_printf.cpp
    kernel/test/test_base.cpp
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/process_test.cpp>
    kernel/test/exception_tester.cpp
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/ata_driver_test.cpp>
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/vfs_test.cpp>
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/block_device_test.cpp>
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/fat32_test.cpp>
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/sync_test.cpp>
    $<$<NOT:$<BOOL:${DISK_BOOT_ONLY}>>:kernel/test/k_printf_test.cpp>
    userspace/programs/test_simple.cpp
    userspace/programs/test_syscall.cpp
    userspace/programs/shell.cpp
    userspace/lib/user_utils.cpp
    userspace/programs/ls.cpp
    userspace/programs/cat.cpp
    userspace/programs/mkdir.cpp
    userspace/programs/rmdir.cpp
    userspace/lib/libkira.cpp
    kernel/memory/memory_manager.cpp
    kernel/memory/virtual_memory.cpp
    kernel/formats/elf.cpp
    kernel/loaders/elf_loader.cpp
    kernel/arch/x86/idt.cpp
    kernel/arch/x86/gdt.cpp
    kernel/arch/x86/tss.cpp
    kernel/interrupts/exceptions.cpp
    kernel/interrupts/pic.cpp
    kernel/interrupts/irq.cpp
    kernel/drivers/keyboard.cpp
    kernel/drivers/timer.cpp
    kernel/drivers/ata.cpp
    kernel/fs/vfs.cpp
    kernel/fs/ramfs.cpp
    kernel/fs/block_device.cpp
    kernel/fs/fat32.cpp
    kernel/display/console.cpp
    kernel/debug/serial_debugger.cpp
    kernel/arch/x86/exception_stubs.s
    kernel/arch/x86/syscall_stub.s
    kernel/arch/x86/usermode_switch.s
)

# Set kernel properties
target_compile_options(kernel_core PRIVATE ${KERNEL_CXX_FLAGS})
target_include_directories(kernel_core PRIVATE 
    include
    userspace/include
    userspace/lib
)

# Create kernel executable (ELF format first)
add_executable(kernel.elf kernel/core/boot/kernel_entry.cpp)

# Link the kernel library to the executable
target_link_libraries(kernel.elf PRIVATE kernel_core)

# Set the same compiler options for the executable
target_compile_options(kernel.elf PRIVATE ${KERNEL_CXX_FLAGS})
target_include_directories(kernel.elf PRIVATE 
    include
    userspace/include
    userspace/lib
)

# Set linker flags for the executable
target_link_options(kernel.elf PRIVATE
    "-nostdlib"
    "-m32"
    "-static"
    "-T${CMAKE_SOURCE_DIR}/kernel/linker.ld"
)

# Set the linker script dependency
set_target_properties(kernel.elf PROPERTIES LINK_DEPENDS "${CMAKE_SOURCE_DIR}/kernel/linker.ld")

# Convert the ELF executable to a flat binary (for custom bootloader)
# We need to create a binary that includes all segments with proper spacing
add_custom_command(TARGET kernel.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary --gap-fill=0x00 kernel.elf kernel.bin
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Converting kernel.elf to flat binary format with proper segment spacing (kernel.bin)"
)

# Create boot directory for kernel binary
add_custom_command(TARGET kernel.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/boot
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/kernel.bin ${CMAKE_BINARY_DIR}/boot/
    COMMENT "Creating boot directory structure"
) 