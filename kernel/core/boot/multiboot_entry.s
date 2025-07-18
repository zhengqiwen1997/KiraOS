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

.section .text.multiboot_start
.global multiboot_start
.type multiboot_start, @function

multiboot_start:
    # Set up stack
    mov $stack_top, %esp
    
    # Save all registers that might contain important data
    push %eax
    push %ebx
    push %ecx
    push %edx
    push %edi
    push %esi
    
    # Check if we have valid multiboot magic
    cmp $0x2BADB002, %eax
    jne no_multiboot_magic
    
    # Reliable detection logic:
    # IMG method: Stage2 sets up specific register values
    #   - EBX = 0x200000 (our known memory map buffer address)
    #   - EDI = entry count (typically 6-8)
    #   - ECX = 0xDEADBEEF (signature from Stage2)
    # ELF method: QEMU/GRUB sets up multiboot info structure
    #   - EBX = pointer to multiboot info structure (varies, but not 0x200000)
    #   - EDI = undefined/random
    #   - ECX = undefined/random
    
    # First check: Is EBX exactly our IMG buffer address?
    cmp $0x200000, %ebx
    jne elf_method
    
    # Second check: Is EDI in reasonable range for memory map entries?
    cmp $50, %edi           # Too many entries
    jg elf_method
    cmp $2, %edi            # Too few entries
    jl elf_method
    
    # Third check: Does ECX contain our Stage2 signature?
    cmp $0xDEADBEEF, %ecx
    jne elf_method
    
    # All checks passed - this is IMG method
    jmp img_method

elf_method:
    # ELF method: Use standard multiboot info structure parsing
    # EBX contains pointer to multiboot info structure
    # We need to extract memory map from the structure
    
    # Clean up stack and set up for multiboot parsing
    pop %esi        # Restore ESI
    pop %edi        # Restore EDI (will be overwritten by extract_multiboot_memory_map)
    pop %edx        # Restore EDX
    pop %ecx        # Restore ECX
    pop %ebx        # Restore EBX (multiboot info pointer)
    pop %eax        # Restore EAX (multiboot magic)
    
    # Call multiboot parsing function
    # Input: EBX = multiboot info pointer
    # Output: EBX = memory map buffer, EDI = entry count
    call extract_multiboot_memory_map
    
    # Call C++ kernel entry point
    call _start
    jmp halt_loop

img_method:
    # IMG method: Memory map is already prepared by Stage2
    # EBX = memory map buffer address (0x200000)
    # EDI = entry count
    
    # Clean up stack but keep EBX and EDI
    pop %esi        # Restore ESI (not needed)
    add $4, %esp    # Skip EDI (keep current value)
    pop %edx        # Restore EDX (not needed)
    pop %ecx        # Restore ECX (not needed)
    add $4, %esp    # Skip EBX (keep current value)
    pop %eax        # Restore EAX (not needed)
    
    # EBX and EDI are already set correctly for _start
    # Call C++ kernel entry point
    call _start
    jmp halt_loop

no_multiboot_magic:
    # No valid multiboot magic - this shouldn't happen
    # Clean up stack
    pop %esi
    pop %edi
    pop %edx
    pop %ecx
    pop %ebx
    pop %eax
    
    # Set default values and continue
    mov $0, %ebx        # No memory map buffer
    mov $0, %edi        # No memory map entries
    call _start
    
halt_loop:
    cli
    hlt
    jmp halt_loop

# Extract memory map from multiboot info structure (ELF method only)
# Input: EBX = multiboot info pointer
# Output: EBX = memory map buffer address, EDI = entry count
extract_multiboot_memory_map:
    push %eax
    push %ecx
    push %edx
    push %esi
    
    mov %ebx, %esi          # ESI = multiboot info pointer
    
    # Check if memory map is available (bit 6 of flags)
    mov (%esi), %eax        # Load flags
    test $0x40, %eax        # Test bit 6 (memory map available)
    jz try_basic_memory
    
    # Get memory map information from multiboot structure
    mov 44(%esi), %ebx      # mmap_addr (offset 44 in multiboot info)
    mov 40(%esi), %ecx      # mmap_length (offset 40 in multiboot info)
    
    # Validate the buffer address and length
    test %ebx, %ebx         # Check if buffer address is valid
    jz try_basic_memory
    test %ecx, %ecx         # Check if length is valid
    jz try_basic_memory
    
    # Calculate entry count: length / 24 (standard multiboot entry size)
    mov %ecx, %eax          # Length in EAX
    xor %edx, %edx          # Clear high part
    mov $24, %ecx           # Divisor
    div %ecx                # EAX = length / 24
    mov %eax, %edi          # Entry count
    
    # Sanity check: reasonable range for memory map entries
    cmp $50, %edi           # If more than 50, something's wrong
    jg use_defaults
    cmp $1, %edi            # If less than 1, something's wrong
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
    
    # Entry 1: Base address (u64) = 0x00000000
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Base address low = 0
    add $4, %edi
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Base address high = 0
    add $4, %edi
    
    # Entry 1: Length (u64) = mem_lower * 1024
    mov 4(%esi), %eax       # Get mem_lower
    shl $10, %eax           # Multiply by 1024
    mov %eax, (%edi)        # Length low
    add $4, %edi
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Length high = 0
    add $4, %edi
    
    # Entry 1: Type (u32) = 1 (available RAM)
    mov $0x00000001, %eax
    mov %eax, (%edi)        # Type = available
    add $4, %edi
    
    # Entry 1: ACPI (u32) = 0 (not used)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # ACPI = 0
    add $4, %edi
    
    # Create memory map entry 2: High memory (0x100000 - mem_upper*1024)
    # Entry 2: Base address (u64) = 0x00100000 (1MB)
    mov $0x00100000, %eax
    mov %eax, (%edi)        # Base address low = 1MB
    add $4, %edi
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Base address high = 0
    add $4, %edi
    
    # Entry 2: Length (u64) = mem_upper * 1024
    mov 8(%esi), %eax       # Get mem_upper
    shl $10, %eax           # Multiply by 1024
    mov %eax, (%edi)        # Length low
    add $4, %edi
    mov $0x00000000, %eax
    mov %eax, (%edi)        # Length high = 0
    add $4, %edi
    
    # Entry 2: Type (u32) = 1 (available RAM)
    mov $0x00000001, %eax
    mov %eax, (%edi)        # Type = available
    add $4, %edi
    
    # Entry 2: ACPI (u32) = 0 (not used)
    mov $0x00000000, %eax
    mov %eax, (%edi)        # ACPI = 0
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

# Stack space
.section .bss
.align 16
stack_bottom:
.skip 16384  # 16KB stack
stack_top:
