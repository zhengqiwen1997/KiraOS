# KiraOS

KiraOS is a self-made operating system written in C++ for fun and study. It features a complete process management system, hardware interrupt handling, and runs on x86 machines. The project demonstrates fundamental OS concepts and is designed to be educational and accessible.

## Key Features

- ✅ **Virtual Memory**: Full paging support with per-process address spaces
- ✅ **System Calls**: INT 0x80 interface for user-kernel communication
- ✅ **Ring 3 User Mode**: Hardware-enforced privilege separation with memory isolation
- ✅ **Process Management**: Round-robin scheduler with complete user/kernel separation
- ✅ **Memory Protection**: Page-level access control and address space isolation
- ✅ **Hardware Interrupts**: Timer (100 Hz) and keyboard handling with proper IRQ routing
- ✅ **Exception Handling**: Comprehensive CPU fault debugging with page fault analysis
- ✅ **ATA/IDE Driver**: Hardware disk I/O with comprehensive testing framework
- ✅ **Dual Boot Methods**: Both ELF kernel loading and custom bootloader support

## Quick Start

```bash
# Build
make clean && make

# Run (ELF method - recommended)
make run

# Run (IMG method - custom bootloader)
make run-disk
```

## Prerequisites

- **i686-elf-gcc** cross-compiler toolchain
- **CMake 3.20+**
- **QEMU** (qemu-system-i386)

## Project Structure

```
KiraOS/
├── kernel/                    # Kernel implementation
│   ├── boot/                  # Stage1 & Stage2 bootloaders (assembly)
│   ├── core/                  # Process management, user mode, system calls
│   ├── arch/x86/              # x86-specific code (GDT, TSS, assembly)
│   ├── interrupts/            # Exception and IRQ handling
│   ├── drivers/               # Timer, keyboard, ATA/IDE drivers
│   ├── memory/                # Virtual memory, paging, physical allocation
│   └── test/                  # Driver tests and validation
├── userspace/                 # User mode programs and libraries
│   ├── programs/              # User applications
│   └── lib/                   # User mode libraries
├── include/                   # Kernel headers
│   ├── drivers/               # Driver interfaces
│   └── test/                  # Test framework headers
├── build/                     # Build output (disk images)
├── cmake-build/               # CMake build directory
├── Makefile                   # Main build system
└── CMakeLists.txt            # CMake configuration
```

## Development

### Build Commands
```bash
make clean && make            # Clean build everything
make                          # Build everything
make clean                    # Clean build files
```

### Running
```bash
# ELF method (recommended) - QEMU loads kernel directly
make run                      # Basic run with console output
make run-with-log            # Run with serial logging

# IMG method - Custom bootloader with disk image
make run-disk                # Boot from disk image
```

## Boot Methods

KiraOS supports two boot methods for different use cases:

### ELF Method (`make run`)
- **Recommended for development** - faster boot, easier debugging
- QEMU loads kernel directly using `-kernel` flag
- Multiboot-compliant kernel with proper ELF sections
- Ideal for testing and development

### IMG Method (`make run-disk`)
- **Real bootloader experience** - complete boot chain
- Custom Stage1 (MBR) and Stage2 bootloaders written in assembly
- Kernel loaded from disk sectors into memory
- More realistic boot process, useful for understanding OS boot sequence

Both methods support the same kernel features and run identical code after boot.

## Architecture

### Memory Management
- **Virtual Memory**: Per-process 4GB address spaces with page tables
- **Physical Allocator**: Stack-based page allocation with memory map parsing
- **Address Translation**: Hardware paging with TLB management
- **Protection**: Page-level read/write/user permissions

### System Interface
- **System Calls**: INT 0x80 with register-based parameter passing
- **User API**: Clean C++ interface (`UserAPI::write()`, etc.)
- **Privilege Transition**: IRET-based Ring 0 ↔ Ring 3 switching

### Core Components
- **Process Scheduler**: Round-robin with address space switching
- **Exception Handling**: Page faults, protection violations, debug info
- **Interrupt System**: IDT with 256 entries, PIC support, timer-driven scheduling

## Contributing

### Current Development Focus
- **File System**: FAT32 implementation on top of ATA driver foundation
- **Storage**: Enhanced disk I/O and partition support
- **User Interface**: Shell and command-line utilities

### Future Areas of Interest
- Network stack and drivers
- Multi-core SMP support  
- Advanced memory management (swap, CoW)

## References

The following resources have been valuable in the development of KiraOS:

- [OSDev Wiki](https://wiki.osdev.org/Expanded_Main_Page)
- [Operating Systems: From 0 to 1](https://github.com/tuhdo/os01/blob/master/Operating_Systems_From_0_to_1.pdf)
- [thor-os](https://github.com/wichtounet/thor-os)
- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf)
- [BrokenThorn OS Development Series](http://www.brokenthorn.com/Resources/OSDevIndex.html)

## License

MIT License - See source for details.