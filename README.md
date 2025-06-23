# KiraOS

KiraOS is a self-made operating system written in C++ for fun and study. It features a complete process management system, hardware interrupt handling, and runs on x86 machines. The project demonstrates fundamental OS concepts and is designed to be educational and accessible.

## Features

- **Complete Process Management**: Round-robin scheduler with process creation, termination, and context switching
- **Hardware Interrupt System**: Timer and keyboard interrupt handling with PIC support
- **Protected Mode Kernel**: Modern C++20 kernel with proper memory management
- **Two-stage Bootloader**: Memory detection and protected mode setup
- **Real-time Display**: Live process monitoring and system statistics
- **Safe String Utilities**: Overlap-safe string operations for kernel stability
- **Modular Architecture**: Clean separation of concerns with organized codebase

## Current System Capabilities

- ✅ **Process Scheduling**: 3 demo processes running concurrently
- ✅ **Timer Interrupts**: 18.2 Hz system timer driving scheduler
- ✅ **Keyboard Input**: Scan code to ASCII conversion with modifier support
- ✅ **Memory Management**: Static process stack allocation and management
- ✅ **Exception Handling**: CPU exception handlers for debugging
- ✅ **VGA Display**: Real-time system status and process visualization

## Project Structure

```
KiraOS/
├── build/                     # Build output directory
├── include/                   # Kernel header files
│   ├── core/                  # Core system headers
│   │   ├── process.hpp        # Process management
│   │   ├── utils.hpp          # Safe utility functions
│   │   └── types.hpp          # System type definitions
│   ├── interrupts/            # Interrupt handling
│   ├── drivers/               # Device drivers
│   └── display/               # Display system
├── kernel/
│   ├── boot/                  # Bootloader stages
│   ├── core/                  # Kernel core implementation
│   │   ├── kernel.cpp         # Main kernel initialization
│   │   ├── process.cpp        # Process management system
│   │   └── test_processes.cpp # Demo processes for scheduler
│   ├── interrupts/            # Interrupt handlers and IDT
│   ├── drivers/               # Hardware drivers
│   └── arch/x86/              # x86-specific code
├── CMakeLists.txt             # CMake build configuration
├── Makefile                   # Main build system
└── README.md                  # This file
```

## Building and Running

### Prerequisites

- NASM (Netwide Assembler)
- QEMU
- i686-elf-gcc cross-compiler toolchain
- CMake 3.20+
- Make

### Building and Running

```bash
# Build everything
make

# Build and run in QEMU
make run

# Run with GDB debugging enabled
make debug

# Clean build files
make clean

# Show help
make help
```

## System Architecture

### Boot Procedure
1. **Stage 1 (MBR)**: 512-byte bootloader loads Stage 2
2. **Stage 2**: A20 line, memory detection, GDT setup, protected mode
3. **Kernel**: C++ kernel initialization and main loop

### Process Management
- **Round-robin Scheduler**: 10-tick time slices (~0.55 seconds)
- **Process States**: READY, RUNNING, BLOCKED, TERMINATED
- **Static Stack Allocation**: 4KB per process, 16 process limit
- **Context Switching**: Full CPU register state preservation

### Interrupt System
- **IDT Setup**: Complete interrupt descriptor table
- **Timer Handler**: Drives process scheduling at 18.2 Hz
- **Keyboard Handler**: PS/2 keyboard with scan code conversion
- **Exception Handlers**: CPU fault handling for debugging

## Development Highlights

### Recent Improvements
- **Code Organization**: Separated test processes into dedicated module
- **Constructor Optimization**: Simplified ProcessManager initialization
- **String Safety**: Overlap-safe string utilities preventing memory corruption
- **Display Cleanup**: Streamlined real-time status information

### Key Design Decisions
- **Static Memory Allocation**: Predictable memory usage for kernel stability
- **Enum Class Usage**: Type-safe process states with explicit sizing
- **Modular Design**: Clear separation between core kernel and utilities

## Next Steps

1. **Virtual Memory**: Paging and memory protection
2. **User Mode**: Privilege separation and system calls
3. **File System**: Basic filesystem implementation
4. **Shell**: Interactive command interface
5. **Network Stack**: Basic networking capabilities

## Debugging

- Serial output saved to `build/serial.log`
- GDB debugging: `make debug` then connect with `gdb -ex "target remote localhost:1234"`
- Real-time system monitoring through VGA display

## Contributing

Contributions welcome! Areas of interest:
- Memory management improvements
- Additional device drivers
- Filesystem implementation
- Performance optimizations

## References

The following resources have been valuable in the development of KiraOS:

- [OSDev Wiki](https://wiki.osdev.org/Expanded_Main_Page)
- [Operating Systems: From 0 to 1](https://github.com/tuhdo/os01/blob/master/Operating_Systems_From_0_to_1.pdf)
- [thor-os](https://github.com/wichtounet/thor-os)
- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf)
- [BrokenThorn OS Development Series](http://www.brokenthorn.com/Resources/OSDevIndex.html)

## License

This project is open source and available under the MIT License.