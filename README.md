# KiraOS

KiraOS is a self-made operating system written in C++ for fun and study. It features a complete process management system, hardware interrupt handling, and runs on x86 machines. The project demonstrates fundamental OS concepts and is designed to be educational and accessible.

## Key Features

- ✅ **Ring 3 User Mode**: Hardware-enforced privilege separation (Ring 0 kernel, Ring 3 user)
- ✅ **Process Management**: Round-robin scheduler with kernel and user processes
- ✅ **Memory Protection**: User programs isolated from kernel memory
- ✅ **Hardware Interrupts**: Timer (100 Hz) and keyboard handling
- ✅ **Exception Handling**: Comprehensive CPU fault debugging
- ✅ **Real-time Monitoring**: Live process status via VGA display

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
│   ├── core/                  # Process management, user mode
│   ├── arch/x86/              # x86-specific code (GDT, TSS, assembly)
│   ├── interrupts/            # Exception and IRQ handling
│   ├── drivers/               # Timer, keyboard drivers
│   └── memory/                # Memory management
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

### Privilege Levels
- **Ring 0**: Kernel mode with full system access
- **Ring 3**: User mode with memory protection
- **Transition**: IRET-based switching with proper segment selectors

### Core Components
- **Process Scheduler**: Round-robin with 10-tick time slices
- **Memory Management**: GDT, TSS, stack isolation
- **Interrupt System**: IDT with 256 entries, PIC support
- **User Mode**: Clean privilege separation with memory protection

## Contributing

Areas of interest:
- System calls implementation
- Virtual memory and paging
- Additional device drivers
- File system support

## References

The following resources have been valuable in the development of KiraOS:

- [OSDev Wiki](https://wiki.osdev.org/Expanded_Main_Page)
- [Operating Systems: From 0 to 1](https://github.com/tuhdo/os01/blob/master/Operating_Systems_From_0_to_1.pdf)
- [thor-os](https://github.com/wichtounet/thor-os)
- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf)
- [BrokenThorn OS Development Series](http://www.brokenthorn.com/Resources/OSDevIndex.html)

## License

MIT License - See source for details.