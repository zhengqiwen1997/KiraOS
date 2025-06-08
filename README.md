# KiraOS

KiraOS is a self-made operating system mainly written in C++ for fun and study. It is designed to run on x86 machines with minimal instruction set requirements. The project aims to be educational and accessible to those interested in OS development.

## Features

- Two-stage bootloader with memory detection
- Protected mode setup with GDT
- Serial port debugging
- Build system with Makefile and shell script options

## Project Structure

```
KiraOS/
├── build/              # Build output directory
├── kernel/
│   └── boot/
│       ├── stage1.asm  # First stage bootloader (MBR)
│       └── stage2.asm  # Second stage bootloader
├── build.sh            # Build script
├── Makefile            # Makefile for building the OS
└── README.md           # This file
```

## Building and Running

### Prerequisites

- NASM (Netwide Assembler)
- QEMU
- GCC (for future C/C++ kernel development)
- Make (optional, for using the Makefile)

### Using the build script

```bash
# Build and run
./build.sh

# Clean build
./build.sh --clean

# Build only, don't run
./build.sh --build

# Run only (using existing image)
./build.sh --run

# Show help
./build.sh --help
```

### Using the Makefile

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

## Debugging

- Serial output is saved to `build/serial.log`
- Use `make debug` to start QEMU with GDB server enabled
- Connect with GDB: `gdb -ex "target remote localhost:1234"`

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