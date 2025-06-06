[BITS 16]
[ORG 0x7E00]

start:
    ; Print stage2 start message
    mov si, msg_start
    call print_string

    ; Enable A20 line
    call enable_a20
    mov si, msg_a20
    call print_string

    ; Load GDT
    lgdt [gdt_descriptor]
    mov si, msg_gdt
    call print_string

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
    jmp 0x08:protected_mode

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

; Print string function (SI = string pointer)
print_string:
    mov ah, 0x0E        ; BIOS teletype function
.loop:
    lodsb               ; Load byte from SI into AL
    test al, al         ; Check if end of string (0)
    jz .done
    int 0x10            ; Print character
    jmp .loop
.done:
    ret

[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, 0x10           ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000       ; Set up stack

    ; Draw a separator line
    mov edi, 0xB8000 + (15 * 160)  ; Line 15
    mov ah, 0x07           ; Gray on black
    mov al, '-'
    mov ecx, 80           ; Screen width
.separator:
    mov [edi], ax
    add edi, 2
    loop .separator

    ; Print protected mode message
    mov edi, 0xB8000 + (16 * 160)  ; Line 16
    mov esi, pm_msg
    call print_pm_string

    ; Print success indicator
    mov edi, 0xB8000 + (17 * 160)  ; Line 17
    mov ah, 0x0F           ; White on black
    mov al, 'P'
    mov [edi], ax
    mov al, 'M'
    mov [edi + 2], ax
    mov al, '!'
    mov [edi + 4], ax

    ; Halt CPU
    cli
    hlt

; Print string in protected mode
; ESI = string pointer, EDI = video memory position
print_pm_string:
    push eax
    mov ah, 0x0F           ; White on black
.loop:
    lodsb                  ; Load byte from ESI into AL
    test al, al
    jz .done
    mov [edi], ax
    add edi, 2
    jmp .loop
.done:
    pop eax
    ret

section .data
    msg_start db 'Stage2: Starting...', 13, 10, 0
    msg_a20 db 'Stage2: A20 line enabled', 13, 10, 0
    msg_gdt db 'Stage2: GDT loaded, entering protected mode...', 13, 10, 0
    pm_msg db 'KiraOS Stage2: Successfully entered protected mode', 0

; GDT
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

; Pad to 2048 bytes
times 2048-($-$$) db 0