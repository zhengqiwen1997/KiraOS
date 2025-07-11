# Multiboot header for GRUB/QEMU compatibility
.set MULTIBOOT_MAGIC, 0x1BADB002
.set MULTIBOOT_FLAGS, 0x00000003
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# Serial port constants
.set COM1_BASE, 0x3F8
.set COM1_DATA, 0x3F8
.set COM1_IER, 0x3F9
.set COM1_LCR, 0x3FB
.set COM1_LSR, 0x3FD

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
    
    # Initialize serial port for basic logging
    call init_serial
    
    # Save multiboot info
    # EAX contains multiboot magic number
    # EBX contains pointer to multiboot info structure
    push %eax
    push %ebx
    
    # Check if we have valid multiboot info
    cmp $0x2BADB002, %eax
    jne no_multiboot
    
    # Extract memory map from multiboot info structure
    call extract_multiboot_memory_map
    jmp call_kernel

no_multiboot:
    # Send error message to serial
    push $msg_no_multiboot
    call print_serial_string
    add $4, %esp
    
    # No multiboot info, set default values
    mov $0, %ebx        # No memory map buffer
    mov $0, %edi        # No memory map entries

call_kernel:
    # Call C++ kernel entry point
    # EBX and EDI are now set up with memory map info
    call _start
    
    # Hang if kernel returns
halt_loop:
    cli
    hlt
    jmp halt_loop

# Extract memory map from multiboot info structure
# Input: multiboot info pointer on stack
# Output: EBX = memory map buffer address, EDI = entry count
extract_multiboot_memory_map:
    push %eax
    push %ecx
    push %edx
    push %esi
    
    # Get multiboot info pointer from stack
    mov 20(%esp), %esi      # multiboot info pointer (accounting for pushed registers + call)
    
    # Check if memory map is available (bit 6 of flags)
    mov (%esi), %eax        # Load flags
    test $0x40, %eax        # Test bit 6 (memory map available)
    jz use_defaults
    
    # Get memory map information from multiboot structure
    mov 44(%esi), %ebx      # mmap_addr (offset 44 in multiboot info)
    mov 40(%esi), %ecx      # mmap_length (offset 40 in multiboot info)
    
    # If mmap_length is 0, try to create a basic memory map from mem_lower/mem_upper instead
    test %ecx, %ecx
    jz try_basic_memory
    
    # Validate the buffer address and length
    test %ebx, %ebx         # Check if buffer address is valid
    jz use_defaults
    test %ecx, %ecx         # Check if length is valid
    jz use_defaults
    
    # Simple calculation: assume each entry is 24 bytes (standard multiboot)
    # Count = length / 24
    mov %ecx, %eax          # Length in EAX
    xor %edx, %edx          # Clear high part
    mov $24, %ecx           # Divisor
    div %ecx                # EAX = length / 24
    mov %eax, %edi          # Entry count
    
    # Sanity check: reasonable range for memory map entries
    cmp $50, %edi           # If more than 50, something's wrong
    jg use_defaults
    cmp $2, %edi            # If less than 2, something's wrong
    jl use_defaults
    
    # EBX already contains buffer address
    # EDI contains calculated entry count
    jmp extract_done

try_basic_memory:
    # mmap_length is 0, so try to create a basic memory map from mem_lower/mem_upper
    
    # Check if we have basic memory info (flags bit 0)
    mov (%esi), %eax        # Load flags
    test $0x01, %eax        # Test bit 0 (mem_lower/mem_upper available)
    jz use_defaults
    
    # We have basic memory info, create a simple memory map
    mov $0x8000, %ebx       # Use safe buffer address
    
    # Create memory map entry 1: Low memory (0x0000 - mem_lower*1024)
    mov %ebx, %edi          # EDI = buffer address
    
    # Entry 1: Size field (20 bytes for entry data)
    mov $20, %eax
    mov %eax, (%edi)        # Size = 20
    add $4, %edi
    
    # Entry 1: Base address (low 32 bits)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Base address low = 0
    add $4, %edi
    
    # Entry 1: Base address (high 32 bits)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Base address high = 0
    add $4, %edi
    
    # Entry 1: Length (low 32 bits) = mem_lower * 1024
    mov 4(%esi), %eax       # Get mem_lower
    shl $10, %eax           # Multiply by 1024
    mov %eax, (%edi)        # Length low
    add $4, %edi
    
    # Entry 1: Length (high 32 bits)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Length high = 0
    add $4, %edi
    
    # Entry 1: Type (1 = available RAM)
    mov $0x00000001, %eax
    mov %eax, (%edi)        # Type = available
    add $4, %edi
    
    # Create memory map entry 2: High memory (0x100000 - mem_upper*1024)
    # Entry 2: Size field (20 bytes for entry data)
    mov $20, %eax
    mov %eax, (%edi)        # Size = 20
    add $4, %edi
    
    # Entry 2: Base address (low 32 bits) = 1MB
    mov $0x00100000, %eax
    mov %eax, (%edi)        # Base address low = 1MB
    add $4, %edi
    
    # Entry 2: Base address (high 32 bits)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Base address high = 0
    add $4, %edi
    
    # Entry 2: Length (low 32 bits) = mem_upper * 1024
    mov 8(%esi), %eax       # Get mem_upper
    shl $10, %eax           # Multiply by 1024
    mov %eax, (%edi)        # Length low
    add $4, %edi
    
    # Entry 2: Length (high 32 bits)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Length high = 0
    add $4, %edi
    
    # Entry 2: Type (1 = available RAM)
    mov $0x00000001, %eax
    mov %eax, (%edi)        # Type = available
    add $4, %edi
    
    # Set return values
    mov $0x8000, %ebx       # Buffer address
    mov $2, %edi            # Entry count
    
    jmp extract_done
    
use_defaults:
    # Fallback values if multiboot info is not available
    mov $0x8000, %ebx       # Safe buffer address
    mov $6, %edi            # Default entry count
    
extract_done:
    pop %esi
    pop %edx
    pop %ecx
    pop %eax
    ret

# Initialize serial port (COM1) for debugging
init_serial:
    push %eax
    push %edx
    
    # Disable all interrupts
    mov $COM1_IER, %dx
    mov $0x00, %al
    out %al, %dx
    
    # Enable DLAB (set baud rate)
    mov $COM1_LCR, %dx
    mov $0x80, %al
    out %al, %dx
    
    # Set baud rate to 38400 (divisor = 3)
    mov $COM1_DATA, %dx
    mov $0x03, %al
    out %al, %dx
    
    mov $COM1_IER, %dx
    mov $0x00, %al
    out %al, %dx
    
    # 8 bits, no parity, 1 stop bit
    mov $COM1_LCR, %dx
    mov $0x03, %al
    out %al, %dx
    
    pop %edx
    pop %eax
    ret

# Print string to serial port
# Input: string address on stack
print_serial_string:
    push %ebp
    mov %esp, %ebp
    push %eax
    push %edx
    push %esi
    
    mov 8(%ebp), %esi       # Get string address from stack
    
.print_loop:
    lodsb                   # Load byte from [ESI] into AL
    test %al, %al           # Check for null terminator
    jz .print_done
    
    call send_serial_char
    jmp .print_loop
    
.print_done:
    pop %esi
    pop %edx
    pop %eax
    pop %ebp
    ret

# Print 32-bit hex value to serial
# Input: value on stack
print_serial_hex32:
    push %ebp
    mov %esp, %ebp
    push %eax
    push %ecx
    push %edx
    
    mov 8(%ebp), %ecx       # Get value from stack
    
    # Print "0x" prefix
    mov $'0', %al
    call send_serial_char
    mov $'x', %al
    call send_serial_char
    
    # Print 8 hex digits
    mov $8, %edx
    
.hex_loop:
    rol $4, %ecx            # Rotate to get next nibble
    mov %ecx, %eax
    and $0x0F, %eax         # Get low nibble
    
    cmp $9, %eax
    jle .hex_digit
    add $7, %eax            # Convert A-F
.hex_digit:
    add $'0', %eax          # Convert to ASCII
    
    call send_serial_char
    
    dec %edx
    jnz .hex_loop
    
    pop %edx
    pop %ecx
    pop %eax
    pop %ebp
    ret

# Send single character to serial port
# Input: character in AL
send_serial_char:
    push %eax
    push %edx
    push %ecx
    
    mov %al, %ah            # Save character
    
    # Wait for transmitter ready
    mov $COM1_LSR, %dx
    mov $0x1000, %ecx       # Timeout counter
    
.wait_ready:
    in %dx, %al
    test $0x20, %al         # Check if transmitter ready
    jnz .send_char
    dec %ecx
    jnz .wait_ready
    jmp .send_done          # Timeout
    
.send_char:
    mov $COM1_DATA, %dx
    mov %ah, %al            # Get character back
    out %al, %dx
    
.send_done:
    pop %ecx
    pop %edx
    pop %eax
    ret

# Debug messages (minimal set)
.section .rodata
msg_no_multiboot:        .asciz "MULTIBOOT: No valid multiboot info\r\n"

# Stack space
.section .bss
.align 16
stack_bottom:
.skip 16384  # 16KB stack
stack_top: