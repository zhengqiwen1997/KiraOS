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
total_memory dd 0       ; Total usable memory in bytes

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

    ; Load GDT
    lgdt [gdt_descriptor]
    mov si, msg_gdt
    call print_string
    call print_serial

    ; Get current cursor position
    mov ah, 0x03
    xor bh, bh
    int 0x10
    push dx          ; Save cursor position for later

    ; Add some newlines to create space
    mov cx, 5        ; Add 5 blank lines
    mov ah, 0x0E
.newlines:
    mov al, 13       ; Carriage return
    int 0x10
    mov al, 10       ; Line feed
    int 0x10
    loop .newlines

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

; Print a single character to serial port
; AL = character
print_serial_char:
    push ax
    push dx
    
    ; Save character
    mov ah, al
    
    ; Wait for transmitter to be empty
    mov dx, 0x3F8 + 5    ; COM1 + Line Status Register
.wait:
    in al, dx
    test al, 0x20        ; Test if transmitter is empty
    jz .wait
    
    ; Restore character and send it
    mov al, ah
    mov dx, 0x3F8        ; COM1 base port
    out dx, al
    
    pop dx
    pop ax
    ret

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

; Print decimal number in AX
print_dec:
    push ax
    push bx
    push cx
    push dx
    
    mov bx, 10              ; Divisor
    xor cx, cx              ; Counter
    
    ; Handle zero case
    test ax, ax
    jnz .not_zero
    
    mov al, '0'
    mov ah, 0x0E
    int 0x10
    jmp .done
    
.not_zero:
    ; Convert number to digits on stack
.push_digits:
    xor dx, dx
    div bx                  ; Divide AX by 10
    push dx                 ; Push remainder (0-9)
    inc cx                  ; Increment digit counter
    test ax, ax             ; Check if quotient is zero
    jnz .push_digits        ; If not, continue
    
    ; Pop digits and print
.pop_digits:
    pop dx                  ; Pop digit
    add dl, '0'             ; Convert to ASCII
    mov al, dl              ; Move to AL for display
    mov ah, 0x0E            ; BIOS teletype function
    int 0x10                ; Print character
    loop .pop_digits        ; Repeat for all digits
    
.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Print decimal number in AX to serial port
print_dec_serial:
    push ax
    push bx
    push cx
    push dx
    
    mov bx, 10              ; Divisor
    xor cx, cx              ; Counter
    
    ; Handle zero case
    test ax, ax
    jnz .not_zero
    
    mov al, '0'
    call print_serial_char
    jmp .done
    
.not_zero:
    ; Convert number to digits on stack
.push_digits:
    xor dx, dx
    div bx                  ; Divide AX by 10
    push dx                 ; Push remainder (0-9)
    inc cx                  ; Increment digit counter
    test ax, ax             ; Check if quotient is zero
    jnz .push_digits        ; If not, continue
    
    ; Pop digits and print
.pop_digits:
    pop dx                  ; Pop digit
    add dl, '0'             ; Convert to ASCII
    mov al, dl              ; Move to AL for display
    call print_serial_char
    loop .pop_digits        ; Repeat for all digits
    
.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Print 32-bit decimal number in EAX
print_dec32:
    push eax
    push ebx
    push ecx
    push edx
    
    mov ebx, 10             ; Divisor
    xor ecx, ecx            ; Counter
    
    ; Handle zero case
    test eax, eax
    jnz .not_zero
    
    mov al, '0'
    mov ah, 0x0E
    int 0x10
    jmp .done
    
.not_zero:
    ; Convert number to digits on stack
.push_digits:
    xor edx, edx
    div ebx                 ; Divide EAX by 10
    push edx                ; Push remainder (0-9)
    inc ecx                 ; Increment digit counter
    test eax, eax           ; Check if quotient is zero
    jnz .push_digits        ; If not, continue
    
    ; Pop digits and print
.pop_digits:
    pop edx                 ; Pop digit
    add dl, '0'             ; Convert to ASCII
    mov al, dl              ; Move to AL for display
    mov ah, 0x0E            ; BIOS teletype function
    int 0x10                ; Print character
    loop .pop_digits        ; Repeat for all digits
    
.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Print 32-bit decimal number in EAX to serial port
print_dec32_serial:
    push eax
    push ebx
    push ecx
    push edx
    
    mov ebx, 10             ; Divisor
    xor ecx, ecx            ; Counter
    
    ; Handle zero case
    test eax, eax
    jnz .not_zero
    
    mov al, '0'
    call print_serial_char
    jmp .done
    
.not_zero:
    ; Convert number to digits on stack
.push_digits:
    xor edx, edx
    div ebx                 ; Divide EAX by 10
    push edx                ; Push remainder (0-9)
    inc ecx                 ; Increment digit counter
    test eax, eax           ; Check if quotient is zero
    jnz .push_digits        ; If not, continue
    
    ; Pop digits and print
.pop_digits:
    pop edx                 ; Pop digit
    add dl, '0'             ; Convert to ASCII
    mov al, dl              ; Move to AL for display
    call print_serial_char
    loop .pop_digits        ; Repeat for all digits
    
.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Print 32-bit hexadecimal number in EAX
print_hex32:
    push eax
    push ebx
    push ecx
    push edx
    
    mov ecx, 8              ; 8 hex digits for 32-bit number
    mov ebx, eax            ; Save original value
    
    ; Print "0x" prefix
    mov al, '0'
    mov ah, 0x0E
    int 0x10
    mov al, 'x'
    int 0x10
    
    ; Print each hex digit
.digit_loop:
    rol ebx, 4              ; Rotate left to get highest digit
    mov al, bl              ; Get lowest 4 bits
    and al, 0x0F            ; Mask off high bits
    
    ; Convert to ASCII
    cmp al, 10
    jb .decimal
    add al, 'A' - 10        ; Convert A-F
    jmp .print
.decimal:
    add al, '0'             ; Convert 0-9
.print:
    mov ah, 0x0E
    int 0x10
    
    loop .digit_loop
    
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Print 32-bit hexadecimal number in EAX to serial port
print_hex32_serial:
    push eax
    push ebx
    push ecx
    push edx
    
    mov ecx, 8              ; 8 hex digits for 32-bit number
    mov ebx, eax            ; Save original value
    
    ; Print "0x" prefix
    mov al, '0'
    call print_serial_char
    mov al, 'x'
    call print_serial_char
    
    ; Print each hex digit
.digit_loop:
    rol ebx, 4              ; Rotate left to get highest digit
    mov al, bl              ; Get lowest 4 bits
    and al, 0x0F            ; Mask off high bits
    
    ; Convert to ASCII
    cmp al, 10
    jb .decimal
    add al, 'A' - 10        ; Convert A-F
    jmp .print
.decimal:
    add al, '0'             ; Convert 0-9
.print:
    call print_serial_char
    
    loop .digit_loop
    
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

;-----------------------------------------------------------------------------
; Memory Detection
;-----------------------------------------------------------------------------

; Detect memory using E820h BIOS function
detect_memory_e820:
    pusha                       ; Save all registers
    
    ; Print debug message
    mov si, msg_e820_start
    call print_string
    call print_serial
    
    ; Set up registers for E820h call
    xor ebx, ebx                ; Clear EBX (continuation value)
    xor bp, bp                  ; BP = entry count
    mov edx, 0x534D4150         ; 'SMAP' in little endian
    mov eax, 0xE820             ; EAX = E820h
    mov di, memory_map_buffer   ; ES:DI points to buffer
    mov [es:di + 20], dword 1   ; Force a valid ACPI 3.X entry
    mov ecx, 24                 ; ECX = 24 byte entry
    
    ; Print registers before call
    push eax
    mov si, msg_e820_regs
    call print_string
    call print_serial
    pop eax
    
    ; Make the call
    int 0x15                    ; Call BIOS
    
    ; Check for error
    jc .error                   ; CF set on error
    
    ; Verify 'SMAP' signature
    cmp eax, 0x534D4150         ; EAX should equal 'SMAP'
    jne .error
    
    ; Check if we got at least one entry
    test ebx, ebx               ; EBX = 0 if only one entry
    jz .single_entry            ; Handle single entry case
    
    ; Process first entry
    jcxz .skip_entry            ; Skip zero-length entries
    cmp cl, 20                  ; Got at least 20 bytes?
    jbe .no_acpi
    test byte [es:di + 20], 1   ; Test ACPI 3.X ignore bit
    je .skip_entry
    
.no_acpi:
    mov ecx, [es:di + 8]        ; Get lower 32 bits of length
    or ecx, [es:di + 12]        ; OR with upper 32 bits
    jz .skip_entry              ; Skip zero-length regions
    
    ; Check if this is usable memory (type 1)
    cmp byte [es:di + 16], 1
    jne .not_usable
    
    ; Add to usable memory count
    add [total_memory], ecx
    
.not_usable:
    inc bp                      ; Increment entry count
    add di, 24                  ; Next entry

.next_entry_loop:
    ; Prepare for next entry
    mov eax, 0xE820             ; EAX = E820h
    mov [es:di + 20], dword 1   ; Force a valid ACPI 3.X entry
    mov ecx, 24                 ; ECX = 24 byte entry
    int 0x15                    ; Call BIOS
    
    ; Check if we're done
    jc .done                    ; CF set means done
    
    ; Process entry
    jcxz .skip_entry_loop       ; Skip zero-length entries
    cmp cl, 20                  ; Got at least 20 bytes?
    jbe .no_acpi_loop
    test byte [es:di + 20], 1   ; Test ACPI 3.X ignore bit
    je .skip_entry_loop
    
.no_acpi_loop:
    mov ecx, [es:di + 8]        ; Get lower 32 bits of length
    or ecx, [es:di + 12]        ; OR with upper 32 bits
    jz .skip_entry_loop         ; Skip zero-length regions
    
    ; Check if this is usable memory (type 1)
    cmp byte [es:di + 16], 1
    jne .not_usable_loop
    
    ; Add to usable memory count
    add [total_memory], ecx
    
.not_usable_loop:
    inc bp                      ; Increment entry count
    add di, 24                  ; Next entry

.skip_entry_loop:
    test ebx, ebx               ; EBX = 0 if list complete
    jne .next_entry_loop        ; Get next entry if not done
    jmp .done
    
.skip_entry:
    inc bp                      ; Still count the entry
    jmp .next_entry_loop        ; Continue with next entry
    
.single_entry:
    ; Process the single entry
    mov ecx, [es:di + 8]        ; Get lower 32 bits of length
    or ecx, [es:di + 12]        ; OR with upper 32 bits
    jz .done                    ; Skip zero-length regions
    
    ; Check if this is usable memory (type 1)
    cmp byte [es:di + 16], 1
    jne .done
    
    ; Add to usable memory count
    add [total_memory], ecx
    inc bp                      ; Increment entry count
    jmp .done

.error:
    ; Print error message with EAX value
    mov si, msg_e820_error
    call print_string
    call print_serial
    
    ; Print EAX value
    mov si, msg_eax_value
    call print_string
    call print_serial
    
    mov eax, eax               ; EAX already has the error code
    call print_hex32
    call print_hex32_serial
    
    ; Use default memory value
    mov dword [total_memory], 640 * 1024  ; 640KB
    mov bp, 0                   ; No entries

.done:
    ; Store entry count
    mov [memory_map_entries], bp
    
    ; Print success message with counts
    mov si, msg_e820_done
    call print_string
    call print_serial
    
    ; Print entry count
    mov ax, bp
    call print_dec
    call print_dec_serial
    
    ; Print total memory in KB
    mov si, msg_e820_memory
    call print_string
    call print_serial
    
    mov eax, [total_memory]
    shr eax, 10                 ; Convert to KB
    call print_dec32
    call print_dec32_serial
    
    ; Print KB suffix
    mov si, msg_kb
    call print_string
    call print_serial
    
    popa                        ; Restore all registers
    ret

;=============================================================================
; GDT (Global Descriptor Table)
;=============================================================================
align 8
gdt_start:
    ; Null descriptor
    dq 0

    ; Code segment descriptor
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit 16-19
    db 0x00         ; Base 24-31

    ; Data segment descriptor
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10010010b    ; Access byte
    db 11001111b    ; Flags + Limit 16-19
    db 0x00         ; Base 24-31
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size
    dd gdt_start                 ; GDT address

;=============================================================================
; Protected Mode Code
;=============================================================================
[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, DATA_SEG       ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000       ; Set up stack

    ; Clear screen
    mov edi, 0xB8000       ; Video memory
    mov ecx, 80 * 25       ; Screen size
    mov ax, 0x0720         ; Space character with gray on black
    rep stosw              ; Fill screen

    ; Draw a separator line
    mov edi, 0xB8000 + (5 * 160)  ; Line 5
    mov ah, 0x1F           ; Blue on white
    mov al, '='
    mov ecx, 80           ; Screen width
.separator:
    mov [edi], ax
    add edi, 2
    loop .separator

    ; Print protected mode message
    mov edi, 0xB8000 + (2 * 160)  ; Line 2
    mov esi, pm_msg
    mov ah, 0x1F           ; Blue on white
    call print_pm_string

    ; Print memory information header
    mov edi, 0xB8000 + (7 * 160)  ; Line 7
    mov esi, pm_msg_header
    mov ah, 0x1F           ; Blue on white
    call print_pm_string
    
    ; Print entry count
    mov edi, 0xB8000 + (9 * 160)  ; Line 9
    mov esi, pm_msg_memory_entries
    mov ah, 0x0F           ; White on black
    call print_pm_string
    
    mov eax, [memory_map_entries]
    mov edi, 0xB8000 + (9 * 160) + 46  ; After "Memory Map Entries Found: "
    mov ah, 0x0A           ; Green on black
    call print_pm_dec
    
    ; Print total memory
    mov edi, 0xB8000 + (11 * 160)  ; Line 11
    mov esi, pm_msg_memory_total
    mov ah, 0x0F           ; White on black
    call print_pm_string
    
    ; Convert bytes to KB for display
    mov eax, [total_memory]
    shr eax, 10                    ; Divide by 1024 to get KB
    mov edi, 0xB8000 + (11 * 160) + 46  ; After "Total Usable Memory: "
    mov ah, 0x0A           ; Green on black
    call print_pm_dec
    
    ; Print "KB"
    mov edi, 0xB8000 + (11 * 160) + 52  ; After the number
    mov ah, 0x0F                   ; White on black
    mov al, 'K'
    mov [edi], ax
    mov al, 'B'
    mov [edi + 2], ax

    ; Halt CPU
    cli
    hlt

;-----------------------------------------------------------------------------
; Protected Mode Print Functions
;-----------------------------------------------------------------------------

; Print string in protected mode
; ESI = string pointer, EDI = video memory position
print_pm_string:
    push eax
    push esi
    push edi
    ; AH already has the color attribute
.loop:
    lodsb                  ; Load byte from ESI into AL
    test al, al
    jz .done
    mov [edi], ax
    add edi, 2
    jmp .loop
.done:
    pop edi
    pop esi
    pop eax
    ret

; Print decimal number in EAX in protected mode
; EDI = video memory position
print_pm_dec:
    push eax
    push ebx
    push ecx
    push edx
    push edi
    
    ; AH already has the color attribute
    mov ebx, 10            ; Divisor
    xor ecx, ecx           ; Digit counter
    
    ; Handle zero case
    test eax, eax
    jnz .not_zero
    
    mov al, '0'
    mov [edi], ax
    add edi, 2
    jmp .done
    
.not_zero:
    ; Convert number to digits on stack
.push_digits:
    xor edx, edx
    div ebx                ; Divide EAX by 10
    push edx               ; Push remainder (0-9)
    inc ecx                ; Increment digit counter
    test eax, eax          ; Check if quotient is zero
    jnz .push_digits       ; If not, continue
    
    ; Pop digits and print
.pop_digits:
    pop edx                ; Pop digit
    add dl, '0'            ; Convert to ASCII
    mov al, dl             ; Move to AL for display
    mov [edi], ax          ; Store in video memory
    add edi, 2             ; Next position
    loop .pop_digits       ; Repeat for all digits
    
.done:
    pop edi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

;=============================================================================
; Data Section
;=============================================================================
section .data
    ; Real mode messages
    msg_start db 'Stage2: Starting...', 13, 10, 0
    msg_a20 db 'Stage2: A20 line enabled', 13, 10, 0
    msg_memory db 'Stage2: Memory detection completed', 13, 10, 0
    msg_gdt db 'Stage2: GDT loaded, entering protected mode...', 13, 10, 0
    
    ; E820h debug messages
    msg_e820_start db 'Starting E820h memory detection...', 13, 10, 0
    msg_e820_regs db 'E820h: EAX=0xE820, EDX=SMAP, ECX=24', 13, 10, 0
    msg_e820_error db 'E820h: Error during detection, using default values', 13, 10, 0
    msg_eax_value db 'E820h: EAX return value: ', 0
    msg_e820_done db 'E820h: Detection complete. Entries: ', 0
    msg_e820_memory db 13, 10, 'E820h: Total memory: ', 0
    msg_kb db ' KB', 13, 10, 0
    
    ; Protected mode messages
    pm_msg db 'KiraOS Stage2: Successfully entered protected mode', 0
    pm_msg_header db 'MEMORY INFORMATION', 0
    pm_msg_memory_entries db 'Memory Map Entries Found: ', 0
    pm_msg_memory_total db 'Total Usable Memory: ', 0

; Pad to 2048 bytes
times 2048-($-$$) db 0