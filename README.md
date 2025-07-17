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

## Quick Start

```bash
# Build
mkdir cmake-build && cd cmake-build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../i686-elf-toolchain.cmake
make

# Run
qemu-system-i386 -kernel kernel.elf -m 32M
```

## Prerequisites

- **i686-elf-gcc** cross-compiler toolchain
- **CMake 3.20+**
- **QEMU** (qemu-system-i386)

## Project Structure

```
KiraOS/
├── kernel/                    # Kernel implementation
│   ├── core/                  # Process management, user mode, system calls
│   ├── arch/x86/              # x86-specific code (GDT, TSS, assembly)
│   ├── interrupts/            # Exception and IRQ handling
│   ├── drivers/               # Timer, keyboard drivers
│   └── memory/                # Virtual memory, paging, physical allocation
├── userspace/                 # User mode programs and libraries
│   ├── programs/              # User applications
│   └── lib/                   # User mode libraries
├── include/                   # Kernel headers
└── CMakeLists.txt            # Build configuration
```

## Development

### Build Commands
```bash
make                          # Build everything
make kernel.elf              # Build kernel only
make clean                    # Clean build
```

### Running & Debugging
```bash
# Basic run
qemu-system-i386 -kernel kernel.elf -m 32M

# With debugging
qemu-system-i386 -kernel kernel.elf -m 32M -s -S
gdb kernel.elf -ex "target remote localhost:1234"

# With serial logging
qemu-system-i386 -kernel kernel.elf -m 32M -serial file:serial.log
```

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

Areas of interest:
- File system implementation
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