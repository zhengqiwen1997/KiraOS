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
KERNEL_SECTORS          equ 64          ; 32KB kernel size (64 * 512 bytes)
KERNEL_TEMP_SEGMENT     equ 0x1000      ; Segment for temporary load (0x10000)

; Memory Detection
MAX_MEMORY_ENTRIES      equ 32          ; Maximum E820 entries to process
MEMORY_ENTRY_SIZE       equ 24          ; Size of each E820 entry
E820_SIGNATURE          equ 0x534D4150  ; 'SMAP' signature
E820_MIN_ENTRY_SIZE     equ 20          ; Minimum valid entry size

; A20 Line Control
A20_BIOS_ENABLE         equ 0x2401      ; BIOS A20 enable function
A20_FAST_GATE_PORT      equ 0x92        ; Fast A20 gate port
A20_FAST_GATE_BIT       equ 0x02        ; A20 enable bit

; Serial Port (COM1) Configuration
COM1_BASE               equ 0x3F8       ; COM1 base port address
COM1_DATA               equ COM1_BASE + 0  ; Data register
COM1_IER                equ COM1_BASE + 1  ; Interrupt Enable Register
COM1_LCR                equ COM1_BASE + 3  ; Line Control Register
COM1_LSR                equ COM1_BASE + 5  ; Line Status Register
SERIAL_BAUD_DIVISOR     equ 3           ; Divisor for 38400 baud
SERIAL_8N1_CONFIG       equ 0x03        ; 8 bits, no parity, 1 stop bit
SERIAL_DLAB_ENABLE      equ 0x80        ; Enable Divisor Latch Access
SERIAL_TX_READY         equ 0x20        ; Transmitter ready bit

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
    ; Initialize serial port for debugging
    call init_serial

    ; Print stage2 start message
    mov si, msg_start
    call print_both

    ; Enable A20 line
    call enable_a20
    mov si, msg_a20
    call print_both

    ; Detect memory map using E820h
    call detect_memory_e820
    mov si, msg_memory
    call print_both
    
    ; Debug: Print memory map count
    call print_memory_map_count

    ; Load kernel from disk
    call load_kernel
    mov si, msg_kernel_loaded
    call print_both

    ; Load GDT
    lgdt [gdt_descriptor]
    mov si, msg_gdt
    call print_both

    ; Switch to protected mode
    call enter_protected_mode
    ; Should never return

;=============================================================================
; Real Mode Functions
;=============================================================================

; Print string function (SI = string pointer)
print_string:
    push ax
    push si
    mov ah, BIOS_TELETYPE   ; BIOS teletype function
.loop:
    lodsb                   ; Load byte from SI into AL
    cmp al, ASCII_NULL      ; Check if end of string
    je .done
    int 0x10                ; Print character
    jmp .loop
.done:
    pop si
    pop ax
    ret

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
    
    ; Set up ES:DI to point to memory map buffer
    mov ax, ds
    mov es, ax
    mov di, memory_map_buffer
    
    ; Initialize
    xor ebx, ebx            ; Start with EBX = 0
    mov word [memory_map_entries], 0
    
    ; Debug: Print E820 start message
    push si
    mov si, msg_e820_start
    call print_both
    pop si

.loop:
    ; Set up E820h call
    mov eax, BIOS_MEMORY_E820
    mov ecx, MEMORY_ENTRY_SIZE
    mov edx, E820_SIGNATURE
    int 0x15
    
    ; Check for errors or unsupported function
    jc .error
    cmp eax, E820_SIGNATURE
    jne .error
    
    ; Valid entry received, increment counter
    inc word [memory_map_entries]
    
    ; Move to next buffer location
    add di, MEMORY_ENTRY_SIZE
    
    ; Check if we've hit our maximum entries
    cmp word [memory_map_entries], MAX_MEMORY_ENTRIES
    jge .done
    
    ; Check if this was the last entry (EBX = 0 after the call means done)
    test ebx, ebx
    jnz .loop               ; If EBX != 0, continue with next entry
    
    jmp .done

.error:
    ; Debug: Print error message
    push si
    mov si, msg_e820_error
    call print_both
    pop si
    
.done:
    ; Debug: Print final count
    push si
    mov si, msg_e820_done
    call print_both
    mov ax, [memory_map_entries]
    call print_hex_word
    mov si, msg_newline
    call print_both
    pop si
    
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Load kernel from disk
load_kernel:
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
    
    ; Copy kernel to final location (1MB)
    call copy_kernel_to_1mb
    
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Copy kernel from temporary location to 1MB
copy_kernel_to_1mb:
    push eax
    push ecx
    push esi
    push edi
    push ds
    push es
    
    ; Disable interrupts during copy
    cli
    
    ; Set up source (0x10000)
    mov ax, KERNEL_TEMP_SEGMENT
    mov ds, ax
    xor esi, esi
    
    ; Set up destination (0x100000)
    ; We need to use 32-bit addressing for 1MB
    xor ax, ax
    mov es, ax
    
    ; Calculate number of bytes to copy
    mov ecx, KERNEL_SECTORS
    shl ecx, 9              ; Multiply by 512 (2^9)
    
    ; Copy loop using 32-bit operations
.copy_loop:
    cmp ecx, 0
    je .copy_done
    
    ; Load from source
    mov al, [ds:esi]
    
    ; Store to destination using direct memory addressing
    ; This is a simplified approach - in reality we'd need to
    ; properly handle the 1MB boundary in real mode
    mov [es:esi + KERNEL_FINAL_ADDR], al
    
    inc esi
    dec ecx
    jmp .copy_loop
    
.copy_done:
    ; Re-enable interrupts
    sti
    
    pop es
    pop ds
    pop edi
    pop esi
    pop ecx
    pop eax
    ret

; Initialize serial port (COM1) for debugging
init_serial:
    push ax
    push dx
    
    ; Disable all interrupts first
    mov dx, COM1_IER
    mov al, 0x00
    out dx, al
    
    ; Enable DLAB (Divisor Latch Access Bit) to set baud rate
    mov dx, COM1_LCR
    mov al, 0x80                ; DLAB = 1
    out dx, al
    
    ; Set baud rate to 9600 (divisor = 12)
    ; Low byte of divisor
    mov dx, COM1_DATA
    mov al, 12                  ; 115200 / 9600 = 12
    out dx, al
    
    ; High byte of divisor
    mov dx, COM1_IER
    mov al, 0x00
    out dx, al
    
    ; Configure line: 8 bits, no parity, 1 stop bit, DLAB = 0
    mov dx, COM1_LCR
    mov al, 0x03                ; 8N1, DLAB = 0
    out dx, al
    
    ; Enable FIFO, clear them, with 14-byte threshold
    mov dx, COM1_BASE + 2       ; FCR (FIFO Control Register)
    mov al, 0xC7
    out dx, al
    
    ; Enable IRQs, set RTS/DSR
    mov dx, COM1_BASE + 4       ; MCR (Modem Control Register)
    mov al, 0x0B
    out dx, al
    
    pop dx
    pop ax
    ret

; Print string to serial port (SI = string pointer)
print_serial:
    push ax
    push dx
    push si
    
.loop:
    lodsb                   ; Load byte from SI into AL
    cmp al, ASCII_NULL      ; Check if end of string
    je .send_newline
    
    call send_serial_char
    jmp .loop
    
.send_newline:
    ; Send carriage return
    mov al, ASCII_CR
    call send_serial_char
    
    ; Send line feed
    mov al, ASCII_LF
    call send_serial_char
    
    pop si
    pop dx
    pop ax
    ret

; Send single character to serial port (AL = character)
send_serial_char:
    push ax
    push dx
    push cx
    
    ; Save the character to send
    mov ah, al
    
    ; Wait for transmitter to be ready
    mov dx, COM1_LSR        ; Line Status Register
    mov cx, 0x1000          ; Timeout counter
.wait:
    in al, dx
    test al, 0x20           ; Test Transmitter Holding Register Empty
    jnz .ready              ; Ready to send
    dec cx
    jnz .wait               ; Continue waiting if not timeout
    jmp .done               ; Timeout, skip sending
    
.ready:
    ; Send character
    mov dx, COM1_DATA       ; Data register
    mov al, ah              ; Get character back
    out dx, al              ; Send character
    
    ; Small delay for character transmission
    mov cx, 100
.delay:
    nop
    loop .delay
    
.done:
    pop cx
    pop dx
    pop ax
    ret

; Print string to both VGA and serial
print_both:
    call print_string
    call print_serial
    ret

; Debug function to print memory map count
print_memory_map_count:
    push ax
    push bx
    push cx
    push dx
    push si
    
    ; Print debug message
    mov si, msg_debug_count
    call print_both
    
    ; Get the memory map count
    mov ax, [memory_map_entries]
    
    ; Convert to hex and print
    call print_hex_word
    
    ; Print newline
    mov si, msg_newline
    call print_both
    
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Print 16-bit value in AX as hex
print_hex_word:
    push ax
    push bx
    push cx
    push dx
    
    mov bx, ax          ; Save original value
    
    ; Print high byte
    mov al, bh
    call print_hex_byte
    
    ; Print low byte
    mov al, bl
    call print_hex_byte
    
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Print 8-bit value in AL as hex
print_hex_byte:
    push ax
    push bx
    push cx
    push dx
    
    mov bl, al          ; Save original value
    
    ; Print high nibble
    shr al, 4
    call print_hex_nibble
    
    ; Print low nibble
    mov al, bl
    and al, 0x0F
    call print_hex_nibble
    
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Print 4-bit value in AL as hex digit
print_hex_nibble:
    push ax
    push bx
    push cx
    push dx
    
    cmp al, 9
    jle .digit
    add al, 7           ; Convert A-F
.digit:
    add al, '0'
    
    ; Print via BIOS (VGA)
    mov ah, BIOS_TELETYPE
    int 0x10
    
    ; Also print to serial
    call send_serial_char
    
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
    or eax, CR0_PROTECTED_MODE
    mov cr0, eax
    
    ; Far jump to flush prefetch queue and load CS
    jmp CODE_SEG:protected_mode_entry

;=============================================================================
; Protected Mode Code
;=============================================================================
[BITS 32]
protected_mode_entry:
    ; Set up data segments
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, BOOTLOADER_STACK
    
    ; Pass memory map information to kernel
    ; EBX = memory map buffer address
    ; EDI = number of entries
    mov ebx, memory_map_buffer
    movzx edi, word [memory_map_entries]
    
    ; Jump to kernel entry point
    jmp KERNEL_FINAL_ADDR

; Write 32-bit value in ECX as hex to VGA at EAX
write_hex32_to_vga:
    push eax
    push ebx
    push ecx
    push edx
    
    mov ebx, eax        ; Save VGA address
    mov edx, 8          ; 8 hex digits
    
.loop:
    rol ecx, 4          ; Rotate to get next nibble
    mov eax, ecx
    and eax, 0x0F       ; Get low nibble
    
    cmp eax, 9
    jle .digit
    add eax, 7          ; Convert A-F
.digit:
    add eax, '0'        ; Convert to ASCII
    
    ; Write to VGA (character + white on black attribute)
    mov ah, 0x0F
    mov [ebx], ax
    add ebx, 2
    
    dec edx
    jnz .loop
    
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

;=============================================================================
; Global Descriptor Table
;=============================================================================
[BITS 16]
gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0
    
    ; Code segment descriptor
    dw 0xFFFF               ; Limit (bits 0-15)
    dw 0x0000               ; Base (bits 0-15)
    db 0x00                 ; Base (bits 16-23)
    db GDT_CODE_ACCESS      ; Access byte
    db GDT_FLAGS            ; Granularity and limit (bits 16-19)
    db 0x00                 ; Base (bits 24-31)
    
    ; Data segment descriptor
    dw 0xFFFF               ; Limit (bits 0-15)
    dw 0x0000               ; Base (bits 0-15)
    db 0x00                 ; Base (bits 16-23)
    db GDT_DATA_ACCESS      ; Access byte
    db GDT_FLAGS            ; Granularity and limit (bits 16-19)
    db 0x00                 ; Base (bits 24-31)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size
    dd gdt_start                ; GDT address

;=============================================================================
; String Messages
;=============================================================================
msg_start           db 'Stage2: Starting KiraOS bootloader', ASCII_CR, ASCII_LF, ASCII_NULL
msg_a20             db 'Stage2: A20 line enabled', ASCII_CR, ASCII_LF, ASCII_NULL
msg_memory          db 'Stage2: Memory map detected', ASCII_CR, ASCII_LF, ASCII_NULL
msg_debug_count     db 'Stage2: Memory entries found: 0x', ASCII_NULL
msg_newline         db ASCII_CR, ASCII_LF, ASCII_NULL
msg_kernel_loaded   db 'Stage2: Kernel loaded', ASCII_CR, ASCII_LF, ASCII_NULL
msg_gdt             db 'Stage2: GDT loaded', ASCII_CR, ASCII_LF, ASCII_NULL
msg_e820_start      db 'Stage2: E820 memory detection started', ASCII_CR, ASCII_LF, ASCII_NULL
msg_e820_error      db 'Stage2: E820 error or unsupported function', ASCII_CR, ASCII_LF, ASCII_NULL
msg_e820_done       db 'Stage2: E820 memory detection complete. Entries found: 0x', ASCII_NULL

; Pad to sector boundary
times 2048-($-$$) db 0