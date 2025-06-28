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
    kernel/core/boot/kernel_entry.cpp
    kernel/core/boot/multiboot_entry.s
    kernel/core/kernel.cpp
    kernel/core/process.cpp
    kernel/core/test_processes.cpp
    kernel/core/syscalls.cpp
    kernel/core/usermode.cpp
    userspace/programs/test_simple.cpp
    userspace/programs/test_syscall.cpp
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
add_custom_command(TARGET kernel.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary kernel.elf kernel.bin
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Converting kernel.elf to flat binary format (kernel.bin)"
)

# Create boot directory for kernel binary
add_custom_command(TARGET kernel.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/boot
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/kernel.bin ${CMAKE_BINARY_DIR}/boot/
    COMMENT "Creating boot directory structure"
) 