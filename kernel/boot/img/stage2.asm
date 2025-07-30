;=============================================================================
; KiraOS Stage2 Bootloader
;=============================================================================
; This is the second stage bootloader for KiraOS. It performs the following:
; 1. Enables A20 line to access memory above 1MB
; 2. Detects system memory using BIOS INT 15h, AX=E820h
; 3. Sets up GDT (Global Descriptor Table)
; 4. Switches to 32-bit protected mode
; 5. Loads and transfers control to kernel
;=============================================================================

[BITS 16]               ; Start in 16-bit real mode
[ORG 0x7E00]            ; Stage2 is loaded at 0x7E00 (right after MBR)

;=============================================================================
; System Constants
;=============================================================================

; Memory Layout
STAGE2_BASE             equ 0x7E00      ; Stage2 load address
KERNEL_TEMP_ADDR        equ 0x10000     ; Temporary kernel load address (64KB)
KERNEL_FINAL_ADDR       equ 0x100000    ; Final kernel address (1MB)
BOOTLOADER_STACK        equ 0x90000     ; Stack at 576KB

; Disk Configuration
BOOT_DRIVE              equ 0x80        ; First hard disk
KERNEL_START_SECTOR     equ 6           ; BIOS sector 6 (disk sector 5)
KERNEL_SECTORS          equ 128         ; 64KB kernel size (128 * 512 bytes)
KERNEL_TEMP_SEGMENT     equ 0x1000      ; Segment for temporary load (0x10000)

; Memory Detection
MAX_MEMORY_ENTRIES      equ 32          ; Maximum E820 entries to process
MEMORY_ENTRY_SIZE       equ 24          ; Size of each E820 entry
E820_SIGNATURE          equ 0x534D4150  ; 'SMAP' signature (ASCII: 'PAMS' in little-endian)
                                                ; Required by BIOS INT 15h, AX=E820h memory detection
E820_MIN_ENTRY_SIZE     equ 20          ; Minimum valid entry size

; A20 Line Control
A20_BIOS_ENABLE         equ 0x2401      ; BIOS A20 enable function
A20_FAST_GATE_PORT      equ 0x92        ; Fast A20 gate port
A20_FAST_GATE_BIT       equ 0x02        ; A20 enable bit

; BIOS Interrupt Functions
BIOS_TELETYPE           equ 0x0E        ; Teletype output function
BIOS_DISK_RESET         equ 0x00        ; Disk reset function
BIOS_DISK_READ          equ 0x02        ; Disk read function
BIOS_MEMORY_E820        equ 0xE820      ; Memory detection function

; Control Register Bits
CR0_PROTECTED_MODE      equ 0x01        ; Protected mode enable bit

; ASCII Control Characters
ASCII_CR                equ 13          ; Carriage return
ASCII_LF                equ 10          ; Line feed
ASCII_NULL              equ 0           ; String terminator

;=============================================================================
; GDT Constants
;=============================================================================

; GDT Segment Selectors
CODE_SEG                equ 0x08        ; Code segment selector
DATA_SEG                equ 0x10        ; Data segment selector

; GDT Access Bytes
GDT_CODE_ACCESS         equ 10011010b   ; Code: present, ring 0, executable, readable
GDT_DATA_ACCESS         equ 10010010b   ; Data: present, ring 0, writable

; GDT Flags
GDT_FLAGS               equ 11001111b   ; 32-bit, page granularity, limit 0xFFFFF

;=============================================================================
; Data Definitions
;=============================================================================

; Memory detection variables
memory_map_entries dw 0 ; Number of memory map entries found

; Memory map buffer (24 bytes per entry, max 32 entries = 768 bytes)
memory_map_buffer times (MAX_MEMORY_ENTRIES * MEMORY_ENTRY_SIZE) db 0

;=============================================================================
; Entry Point
;=============================================================================
start:
    ; Enable A20 line
    call enable_a20

    ; Detect memory map using E820h
    call detect_memory_e820
    
    ; Load kernel from disk to temporary location
    call load_kernel_to_temp
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Switch to protected mode and copy kernel
    call enter_protected_mode
    ; Should never return

;=============================================================================
; Real Mode Functions
;=============================================================================

; Simple A20 line enable
enable_a20:
    push ax
    
    ; Try BIOS method first
    mov ax, A20_BIOS_ENABLE
    int 0x15
    
    ; Try Fast A20 method
    in al, A20_FAST_GATE_PORT
    or al, A20_FAST_GATE_BIT
    out A20_FAST_GATE_PORT, al
    
    pop ax
    ret

; Detect system memory using E820h
detect_memory_e820:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    
    ; Set up buffer
    mov ax, 0x7000
    mov es, ax
    mov di, 0x0000          ; ES:DI = 0x7000:0x0000 (buffer at 0x70000)
    
    xor ebx, ebx            ; EBX = 0 (start of list)
    mov edx, E820_SIGNATURE ; EDX = 'SMAP'
    mov word [memory_map_entries], 0
    
.loop:
    mov eax, 0xE820         ; Function E820h
    mov ecx, MEMORY_ENTRY_SIZE ; Buffer size (24 bytes)
    int 0x15                ; Call BIOS
    
    ; Check for error
    jc .done
    
    ; Check signature
    cmp eax, E820_SIGNATURE
    jne .done
    
    ; Check minimum entry size
    cmp ecx, E820_MIN_ENTRY_SIZE
    jb .skip_entry
    
    ; Copy entry to our buffer
    push si
    push di
    push cx
    
    mov si, di              ; Source: ES:DI (BIOS buffer)
    mov ax, ds
    mov es, ax
    mov di, memory_map_buffer
    mov ax, [memory_map_entries]
    mov cx, MEMORY_ENTRY_SIZE
    mul cx
    add di, ax              ; DI = buffer + (entries * entry_size)
    
    mov cx, MEMORY_ENTRY_SIZE
    rep movsb               ; Copy entry
    
    pop cx
    pop di
    pop si
    
    ; Increment entry count
    inc word [memory_map_entries]
    
    ; Check if we've reached maximum entries
    cmp word [memory_map_entries], MAX_MEMORY_ENTRIES
    jae .done
    
.skip_entry:
    ; Check if this was the last entry
    test ebx, ebx
    jz .done
    
    ; Continue to next entry
    jmp .loop
    
.done:
    ; Restore ES to data segment
    mov ax, ds
    mov es, ax
    
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Load kernel from disk to temporary location
load_kernel_to_temp:
    push ax
    push bx
    push cx
    push dx
    push es
    
    ; Reset disk system
    mov ah, BIOS_DISK_RESET
    mov dl, BOOT_DRIVE
    int 0x13
    
    ; Set up for kernel load to temporary location
    mov ax, KERNEL_TEMP_SEGMENT
    mov es, ax
    xor bx, bx              ; ES:BX = 0x10000 (64KB)
    
    ; Load kernel sectors
    mov ah, BIOS_DISK_READ
    mov al, KERNEL_SECTORS  ; Number of sectors to read
    mov ch, 0               ; Cylinder 0
    mov cl, KERNEL_START_SECTOR ; Starting sector
    mov dh, 0               ; Head 0
    mov dl, BOOT_DRIVE      ; Drive
    int 0x13
    
    ; Don't copy yet - we'll do it in protected mode
    
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Enter protected mode
enter_protected_mode:
    cli                     ; Disable interrupts
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Jump to protected mode code
    jmp 0x08:protected_mode_start

;=============================================================================
; Protected Mode Code
;=============================================================================

[BITS 32]
protected_mode_start:
    ; Set up data segments
    mov ax, 0x10            ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, BOOTLOADER_STACK
    
    ; Copy kernel to final location
    call copy_kernel_protected_mode
    
    ; Copy memory map to kernel-accessible location
    call copy_memory_map_to_kernel
    
    ; Pass memory map information to kernel
    ; EBX = memory map buffer address (now in kernel space)
    ; EDI = number of entries
    ; ECX = signature to identify IMG method
    mov ebx, 0x200000       ; Memory map copied to 2MB (kernel structures area)
    movzx edi, word [memory_map_entries]
    mov ecx, 0xDEADBEEF     ; IMG method signature for reliable detection
                            ; This unique value helps multiboot_entry.s distinguish
                            ; between IMG method (custom bootloader) and ELF method (QEMU)
    
    ; Set up multiboot magic number for kernel compatibility
    mov eax, 0x2BADB002     ; Multiboot magic number (required by multiboot specification)
                            ; This tells the kernel it was loaded by a multiboot-compliant bootloader
    
    ; Jump to kernel entry point
    jmp KERNEL_FINAL_ADDR

; Copy kernel in protected mode (much simpler and more reliable)
copy_kernel_protected_mode:
    push eax
    push ecx
    push esi
    push edi
    
    ; Set up source and destination
    mov esi, KERNEL_TEMP_ADDR       ; Source: 0x10000
    mov edi, KERNEL_FINAL_ADDR      ; Destination: 0x100000
    
    ; Calculate number of bytes to copy
    mov ecx, KERNEL_SECTORS
    shl ecx, 9                      ; Multiply by 512 (2^9)
    
    ; Copy using 32-bit operations (4 bytes at a time for efficiency)
    shr ecx, 2                      ; Divide by 4 (copy DWORDs)
    rep movsd                       ; Copy ECX DWORDs from ESI to EDI
    
    ; Handle any remaining bytes (should be 0 since kernel size is sector-aligned)
    mov ecx, KERNEL_SECTORS
    shl ecx, 9                      ; Total bytes
    and ecx, 3                      ; Remaining bytes (should be 0)
    rep movsb                       ; Copy remaining bytes
    
    pop edi
    pop esi
    pop ecx
    pop eax
    ret

; Copy memory map to kernel-accessible location
copy_memory_map_to_kernel:
    push eax
    push ecx
    push esi
    push edi
    
    ; Source: memory_map_buffer (in Stage2 data)
    mov esi, memory_map_buffer
    
    ; Destination: 2MB (kernel structures area)
    mov edi, 0x200000
    
    ; Calculate bytes to copy
    movzx eax, word [memory_map_entries]
    mov ecx, MEMORY_ENTRY_SIZE
    mul ecx                         ; EAX = entries * entry_size
    mov ecx, eax                    ; ECX = total bytes
    
    ; Copy memory map
    rep movsb
    
    pop edi
    pop esi
    pop ecx
    pop eax
    ret

;=============================================================================
; Global Descriptor Table
;=============================================================================

gdt_start:
    ; Null descriptor (required by x86 architecture)
    dd 0x00000000
    dd 0x00000000
    
    ; Code segment descriptor (Ring 0, 32-bit, 4GB limit)
    dw 0xFFFF       ; Limit (low 16 bits) = 0xFFFF
    dw 0x0000       ; Base (low 16 bits) = 0x0000
    db 0x00         ; Base (middle 8 bits) = 0x00
    db 10011010b    ; Access byte: Present(1) + DPL(00) + S(1) + Type(1010)
                    ; Bit 7: Present = 1 (segment is present)
                    ; Bit 6-5: DPL = 00 (Ring 0 privilege level)
                    ; Bit 4: S = 1 (code/data segment, not system)
                    ; Bit 3: Executable = 1 (code segment)
                    ; Bit 2: Direction = 0 (grows up)
                    ; Bit 1: Readable = 1 (code can be read)
                    ; Bit 0: Accessed = 0 (not accessed yet)
    db 11001111b    ; Granularity + Limit (high 4 bits)
                    ; Bit 7: Granularity = 1 (4KB granularity)
                    ; Bit 6: Size = 1 (32-bit segment)
                    ; Bit 5: Long = 0 (not 64-bit)
                    ; Bit 4: AVL = 0 (available for system use)
                    ; Bit 3-0: Limit high = 1111 (makes limit 0xFFFFF)
    db 0x00         ; Base (high 8 bits) = 0x00
    
    ; Data segment descriptor (Ring 0, 32-bit, 4GB limit)
    dw 0xFFFF       ; Limit (low 16 bits) = 0xFFFF
    dw 0x0000       ; Base (low 16 bits) = 0x0000
    db 0x00         ; Base (middle 8 bits) = 0x00
    db 10010010b    ; Access byte: Present(1) + DPL(00) + S(1) + Type(0010)
                    ; Bit 7: Present = 1 (segment is present)
                    ; Bit 6-5: DPL = 00 (Ring 0 privilege level)
                    ; Bit 4: S = 1 (code/data segment, not system)
                    ; Bit 3: Executable = 0 (data segment)
                    ; Bit 2: Direction = 0 (grows up)
                    ; Bit 1: Writable = 1 (data can be written)
                    ; Bit 0: Accessed = 0 (not accessed yet)
    db 11001111b    ; Granularity + Limit (high 4 bits) - same as code segment
    db 0x00         ; Base (high 8 bits) = 0x00
    
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size
    dd gdt_start                ; GDT address

; Fill remaining space with zeros
times 2048-($-$$) db 0