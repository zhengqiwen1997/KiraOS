#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build

# Assemble the bootloader
nasm -f bin kernel/boot/boot.asm -o build/boot.bin

# Create a disk image (1.44MB floppy)
dd if=/dev/zero of=build/disk.img bs=1024 count=1440
dd if=build/boot.bin of=build/disk.img conv=notrunc

# Run in QEMU
qemu-system-x86_64 -drive format=raw,file=build/disk.img 