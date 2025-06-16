;=============================================================================
; KiraOS Stage2 Bootloader
;=============================================================================
; This is the second stage bootloader for KiraOS. It performs the following:
; 1. Enables A20 line to access memory above 1MB
; 2. Detects system memory using BIOS INT 15h, AX=E820h
; 3. Sets up GDT (Global Descriptor Table)
; 4. Switches to 32-bit protected mode
; 5. Displays system information
;=============================================================================

[BITS 16]               ; Start in 16-bit real mode
[ORG 0x7E00]            ; Stage2 is loaded at 0x7E00 (right after MBR)

;=============================================================================
; Constants and Data Definitions
;=============================================================================

; GDT segment selectors
CODE_SEG equ 0x08       ; Code segment selector (first segment after null)
DATA_SEG equ 0x10       ; Data segment selector (second segment after null)

; Memory detection variables
memory_map_entries dw 0 ; Number of memory map entries found

; Memory map buffer (24 bytes per entry, max 10 entries = 240 bytes)
memory_map_buffer times 240 db 0

;=============================================================================
; Entry Point
;=============================================================================
start:
    ; Initialize serial port for debugging
    call init_serial
    
    ; Print stage2 start message
    mov si, msg_start
    call print_string
    call print_serial

    ; Enable A20 line
    call enable_a20
    mov si, msg_a20
    call print_string
    call print_serial

    ; Detect memory map using E820h
    call detect_memory_e820
    mov si, msg_memory
    call print_string
    call print_serial

    ; Load kernel from disk before entering protected mode
    call load_kernel
    mov si, msg_kernel_loaded
    call print_string
    call print_serial

    ; Load GDT
    lgdt [gdt_descriptor]
    mov si, msg_gdt
    call print_string
    call print_serial

    ; Switch to protected mode
    cli                     ; Disable interrupts
    mov eax, cr0
    or al, 1               ; Set PE bit
    mov cr0, eax

    ; Flush CPU pipeline with far jump
    jmp CODE_SEG:protected_mode

;=============================================================================
; Real Mode Functions
;=============================================================================

;-----------------------------------------------------------------------------
; Print Functions
;-----------------------------------------------------------------------------

; Print string function (SI = string pointer)
print_string:
    push ax                  ; Save registers
    push si
    mov ah, 0x0E            ; BIOS teletype function
.loop:
    lodsb                   ; Load byte from SI into AL
    test al, al             ; Check if end of string (0)
    jz .done
    int 0x10                ; Print character
    jmp .loop
.done:
    pop si                  ; Restore registers
    pop ax
    ret

;-----------------------------------------------------------------------------
; Enable A20 Line
;-----------------------------------------------------------------------------
enable_a20:
    ; Try BIOS method first
    mov ax, 0x2401
    int 0x15
    jnc .done              ; If successful, return

    ; Try Fast A20 method
    in al, 0x92
    or al, 2
    out 0x92, al

.done:
    ret

;-----------------------------------------------------------------------------
; Serial Port Functions
;-----------------------------------------------------------------------------

; Initialize serial port (COM1)
init_serial:
    ; Save registers
    push ax
    push dx
    
    ; Initialize COM1 (port 0x3F8)
    mov dx, 0x3F8 + 1    ; COM1 + LCR offset
    mov al, 0x00         ; Disable all interrupts
    out dx, al
    
    mov dx, 0x3F8 + 3    ; COM1 + LCR offset
    mov al, 0x80         ; Enable DLAB (set baud rate divisor)
    out dx, al
    
    mov dx, 0x3F8 + 0    ; COM1 + Divisor Latch Low Byte
    mov al, 0x03         ; Set divisor to 3 (38400 baud)
    out dx, al
    
    mov dx, 0x3F8 + 1    ; COM1 + Divisor Latch High Byte
    mov al, 0x00         ; High byte of divisor
    out dx, al
    
    mov dx, 0x3F8 + 3    ; COM1 + LCR offset
    mov al, 0x03         ; 8 bits, no parity, one stop bit
    out dx, al
    
    ; Restore registers
    pop dx
    pop ax
    ret

; Print string to serial port (COM1)
; SI = string pointer
print_serial:
    push ax
    push dx
    push si
    
    mov dx, 0x3F8        ; COM1 base port
    
.loop:
    lodsb                ; Load byte from SI into AL
    test al, al          ; Check if end of string (0)
    jz .done
    
    ; Wait for transmitter to be empty
.wait:
    push ax
    mov dx, 0x3F8 + 5    ; COM1 + Line Status Register
    in al, dx
    test al, 0x20        ; Test if transmitter is empty
    pop ax
    jz .wait
    
    ; Send character
    mov dx, 0x3F8        ; COM1 base port
    out dx, al
    jmp .loop
    
.done:
    ; Send newline
    mov al, 13           ; Carriage return
    mov dx, 0x3F8
    out dx, al
    
    mov al, 10           ; Line feed
    mov dx, 0x3F8
    out dx, al
    
    pop si
    pop dx
    pop ax
    ret

; Print hex value to serial port
; AX = value to print
print_hex_serial:
    push ax
    push bx
    push cx
    push dx
    
    mov cx, 4               ; Print 4 hex digits
    mov bx, ax              ; Save value in BX
    
.hex_loop:
    rol bx, 4               ; Rotate left 4 bits to get next digit
    mov al, bl              ; Get low 4 bits
    and al, 0x0F            ; Mask to get only 4 bits
    
    ; Convert to ASCII
    cmp al, 9
    jle .digit
    add al, 'A' - 10        ; Convert A-F
    jmp .print_digit
.digit:
    add al, '0'             ; Convert 0-9
    
.print_digit:
    ; Wait for transmitter to be empty
.wait_hex:
    push ax
    mov dx, 0x3F8 + 5       ; COM1 + Line Status Register
    in al, dx
    test al, 0x20           ; Test if transmitter is empty
    pop ax
    jz .wait_hex
    
    ; Send character
    mov dx, 0x3F8           ; COM1 base port
    out dx, al
    
    loop .hex_loop
    
    ; Send newline
    mov al, 13              ; Carriage return
    mov dx, 0x3F8
    out dx, al
    mov al, 10              ; Line feed
    mov dx, 0x3F8
    out dx, al
    
    pop dx
    pop cx
    pop bx
    pop ax
    ret

;-----------------------------------------------------------------------------
; Kernel Loading Function
;-----------------------------------------------------------------------------

; Load kernel from disk using BIOS INT 13h
load_kernel:
    push ax
    push bx
    push cx
    push dx
    push es
    
    ; Debug: Starting disk load
    mov si, msg_disk_start
    call print_string
    call print_serial
    
    ; Reset disk system first
    mov ah, 0x00            ; BIOS reset disk function
    mov dl, 0x80            ; Use hard-coded drive number (first hard disk)
    int 0x13                ; Call BIOS disk service
    jc .error               ; Jump if carry flag set (error)
    
    ; Set up destination segment (0x0400 = 0x4000 linear address)
    mov ax, 0x0400
    mov es, ax
    xor bx, bx              ; ES:BX = 0x0400:0x0000 = 0x4000 linear
    
    ; Read kernel from sector 6 (BIOS sectors start from 1, disk sector 5 = BIOS sector 6)
    mov ah, 0x02            ; BIOS read sectors function
    mov al, 16              ; Number of sectors to read (8KB for C++ kernel)
    mov ch, 0               ; Cylinder 0
    mov cl, 6               ; Starting sector (BIOS sector 6 = disk sector 5)
    mov dh, 0               ; Head 0
    mov dl, 0x80            ; Use hard-coded drive number (first hard disk)
    int 0x13                ; Call BIOS disk service
    jc .error               ; Jump if carry flag set (error)
    
    ; Debug: Disk load successful
    mov si, msg_disk_success
    call print_string
    call print_serial
    
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.error:
    mov si, msg_disk_error
    call print_string
    call print_serial
    jmp $                   ; Halt on error

;-----------------------------------------------------------------------------
; Memory Detection Function
;-----------------------------------------------------------------------------

; Detect memory using INT 15h, AX=E820h
detect_memory_e820:
    push ax
    push bx
    push cx
    push dx
    push es
    push di
    
    ; Initialize memory map counter
    mov word [memory_map_entries], 0
    
    ; Set up ES:DI to point to our buffer
    mov ax, 0x0000
    mov es, ax
    mov di, memory_map_buffer
    
    ; Initialize registers for INT 15h, AX=E820h
    xor ebx, ebx            ; Start with first entry
    mov edx, 0x534D4150     ; 'SMAP' signature
    mov ecx, 24             ; Request 24 bytes per entry
    
.loop:
    mov eax, 0xE820         ; INT 15h, AX=E820h
    int 0x15                ; Call BIOS memory detection
    jc .done                ; If carry set, we're done
    
    ; Check if we got a valid entry
    cmp eax, 0x534D4150     ; Check 'SMAP' signature
    jne .done               ; If not, we're done
    
    ; Check if we got valid data (ECX should be >= 20)
    cmp ecx, 20
    jb .skip_entry          ; Skip if less than 20 bytes returned
    
    ; Increment entry counter
    inc word [memory_map_entries]
    
    ; Move to next buffer position
    add di, 24              ; Each entry is 24 bytes
    
    ; Check if we've reached max entries
    cmp word [memory_map_entries], 20
    jae .done               ; If we have 20 entries, we're done
    
.skip_entry:
    ; Check if this was the last entry (EBX = 0 means done)
    test ebx, ebx
    jz .done                ; If EBX is 0, we're done
    
    ; Reset ECX for next iteration
    mov ecx, 24             ; Request 24 bytes per entry
    jmp .loop               ; Otherwise, get next entry
    
.done:
    ; Debug: Print final count
    push ax
    push si
    mov si, debug_final_msg
    call print_serial
    mov ax, [memory_map_entries]
    call print_hex_serial
    pop si
    pop ax
    
    pop di
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

;=============================================================================
; Protected Mode Code
;=============================================================================

[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, 0x90000       ; Set stack pointer to 0x90000
    
    ; Pass memory map to kernel
    ; EBX = memory map buffer address
    ; EDI = number of entries (from EDI which was loaded before mode switch)
    mov ebx, memory_map_buffer  ; Use the symbol directly
    movzx edi, word [memory_map_entries]  ; Load entry count into EDI
    
    ; Debug: Send a character to serial before jumping
    mov al, 'J'             ; Send 'J' for Jump
    mov dx, 0x3F8           ; COM1 port
    out dx, al
    
    ; Jump to kernel
    jmp 0x4000             ; Jump to kernel at 0x4000

;=============================================================================
; Data Section
;=============================================================================

; Messages
msg_start:          db 'Stage2: Starting...', 0
msg_a20:            db 'Stage2: A20 line enabled', 0
msg_memory:         db 'Stage2: Memory map detected', 0
msg_kernel_loaded:  db 'Stage2: Kernel loaded at 0x4000', 0
msg_gdt:            db 'Stage2: GDT loaded', 0
msg_disk_start:     db 'Stage2: Loading kernel from disk...', 0
msg_disk_success:   db 'Stage2: Kernel loaded successfully', 0
msg_disk_error:     db 'Stage2: Error loading kernel', 0
debug_final_msg:    db 'Final memory entries: ', 0

; GDT
gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0
    
    ; Code segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10011010b    ; Access byte
    db 11001111b    ; Flags and Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)
    
    ; Data segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10010010b    ; Access byte
    db 11001111b    ; Flags and Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size of GDT
    dd gdt_start                ; Address of GDT

; Pad to 2048 bytes (4 sectors)
times 2048-($-$$) db 0