# KiraOS

KiraOS is a self-made operating system mainly written in C++ for fun and study. It is designed to run on x86 machines with minimal instruction set requirements. The project aims to be educational and accessible to those interested in OS development.

## Features

- Two-stage bootloader with memory detection
- Protected mode setup with GDT
- Serial port debugging
- Build system with Makefile and CMake integration
- Modern C++20 kernel (in development)

## Project Structure

```
KiraOS/
├── build/              # Build output directory
├── include/            # Kernel header files
├── kernel/
│   ├── boot/
│   │   ├── stage1.asm  # First stage bootloader (MBR)
│   │   └── stage2.asm  # Second stage bootloader
│   ├── core/           # Kernel core implementation
│   │   ├── kernel_entry.cpp  # Kernel entry point
│   │   └── kernel.cpp  # Kernel implementation
│   └── include/        # Internal kernel headers
├── Makefile            # Makefile for building the OS
└── README.md           # This file
```

## Building and Running

### Prerequisites

- NASM (Netwide Assembler)
- QEMU
- i686-elf-gcc cross-compiler toolchain
- CMake (for C++ kernel compilation)
- Make

### Building and Running

```bash
# Build everything
make

# Build and run
make run

# Run with GDB debugging enabled
make debug

# Clean build files
make clean

# Show help
make help
```

## Boot Procedure

KiraOS uses a two-stage bootloader:

1. **Stage 1 (MBR)**: Fits in 512 bytes, loads Stage 2 from disk
2. **Stage 2**: Sets up:
   - A20 line enabling
   - Memory detection using INT 15h, AX=E820h
   - GDT (Global Descriptor Table)
   - Protected mode switch
3. **Kernel**: C++ kernel (loaded at 0x4000) with:
   - VGA text mode output
   - Hardware abstraction layer (in development)
   - Memory management (in development)
   - Device drivers (in development)

## Debugging

- Serial output is saved to `build/serial.log`
- Use `make debug` to start QEMU with GDB server enabled
- Connect with GDB: `gdb -ex "target remote localhost:1234"`

## Next Steps

The following features are planned for upcoming development:

1. **Memory Management**:
   - Physical memory manager using the E820h memory map
   - Virtual memory and paging setup
   - Kernel heap allocator

2. **Interrupts**:
   - IDT (Interrupt Descriptor Table) setup
   - Exception handlers
   - Hardware interrupt handlers

3. **Device Drivers**:
   - Keyboard driver
   - Display driver
   - PIC (Programmable Interrupt Controller) initialization

4. **File System**:
   - Basic in-memory file system
   - Disk I/O routines

## Contributing

Anyone is welcome to contribute by:
- Creating issues for bugs or feature requests
- Submitting pull requests
- Providing suggestions for improvements

## References

The following resources have been valuable in the development of KiraOS:

- [OSDev Wiki](https://wiki.osdev.org/Expanded_Main_Page)
- [Operating Systems: From 0 to 1](https://github.com/tuhdo/os01/blob/master/Operating_Systems_From_0_to_1.pdf)
- [thor-os](https://github.com/wichtounet/thor-os)
- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf)
- [BrokenThorn OS Development Series](http://www.brokenthorn.com/Resources/OSDevIndex.html)

## License

This project is open source and available under the MIT License.