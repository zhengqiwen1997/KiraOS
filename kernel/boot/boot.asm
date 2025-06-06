[ORG 0x7c00]


[SECTION .text]
[BITS 16]

global _start
_start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00      ; Set up stack just below where we're loaded

    ; Clear screen using BIOS
    mov ax, 0x0003      ; Text mode 80x25, 16 colors
    int 0x10

    ; Print boot message
    mov si, boot_msg    ; Load message address
    call print_string   ; Call our print routine

    ; Infinite loop - replace this later with actual kernel loading
    jmp $

; Function to print a string
print_string:
    push ax
    push si
    mov ah, 0x0E        ; BIOS teletype output
.loop:
    lodsb               ; Load next character
    test al, al         ; Check if end of string (0)
    jz .done
    int 0x10            ; Print character
    jmp .loop
.done:
    pop si
    pop ax
    ret

; Data
boot_msg db 'KiraOS Bootloader started...', 13, 10, 0

; Boot signature
times 510-($-$$) db 0   ; Pad with zeros until 510 bytes
dw 0xAA55               ; Boot signature
    