# Multiboot header for GRUB/QEMU compatibility
.set MULTIBOOT_MAGIC, 0x1BADB002
.set MULTIBOOT_FLAGS, 0x00000003
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.section .multiboot
.align 4
multiboot_header:
    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS  
    .long MULTIBOOT_CHECKSUM

.section .text
.global multiboot_start
.type multiboot_start, @function

multiboot_start:
    # Set up stack
    mov $stack_top, %esp
    
    # Call C++ kernel entry point
    call _start
    
    # Hang if kernel returns
halt_loop:
    cli
    hlt
    jmp halt_loop

# Stack space
.section .bss
.align 16
stack_bottom:
.skip 16384  # 16KB stack
stack_top: 