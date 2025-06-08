[BITS 16]
[ORG 0x7C00]

; ===== Constants =====
STAGE2_SEGMENT equ 0x07E0    ; Segment where stage2 will be loaded (0x7E00)
STAGE2_SECTORS equ 32        ; Number of sectors to read for stage2 (16KB)
BOOT_DRIVE    equ 0x80       ; First hard drive

start:
    ; Initialize segment registers and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Save boot drive number
    mov [boot_drive], dl

    ; Clear screen
    mov ax, 0x0003          ; AH=0 (set video mode), AL=3 (80x25 text)
    int 0x10

    ; Print boot message
    mov si, msg_boot
    call print_string

    ; Reset disk system
    call reset_disk
    jc disk_error

    ; Load stage2
    call load_stage2
    jc disk_error

    ; Print success message
    mov si, msg_ok
    call print_string

    ; Jump to stage2
    jmp STAGE2_SEGMENT:0

; ===== Functions =====

; Reset disk controller
; Output: CF set on error
reset_disk:
    mov ah, 0               ; Reset disk function
    mov dl, [boot_drive]    ; Drive number
    int 0x13
    ret

; Load stage2 from disk
; Output: CF set on error
load_stage2:
    mov si, msg_loading
    call print_string

    mov ax, STAGE2_SEGMENT  ; Target segment
    mov es, ax
    xor bx, bx              ; Target offset

    mov byte [retry_count], 3

.retry:
    mov ah, 0x02            ; Read sectors function
    mov al, STAGE2_SECTORS  ; Number of sectors to read
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Start from sector 2
    xor dh, dh              ; Head 0
    mov dl, [boot_drive]    ; Drive number
    int 0x13
    jnc .success            ; If success, return

    ; Reset and retry
    call reset_disk
    dec byte [retry_count]
    jnz .retry

    ; Set CF to indicate error
    stc
    ret

.success:
    ; Clear CF to indicate success
    clc
    ret

; Print string (SI = string pointer)
print_string:
    push ax
    push si
    mov ah, 0x0E            ; BIOS teletype function
.loop:
    lodsb                   ; Load byte from SI into AL
    test al, al             ; Check if end of string (0)
    jz .done
    int 0x10                ; Print character
    jmp .loop
.done:
    pop si
    pop ax
    ret

; Handle disk error
disk_error:
    mov si, msg_error
    call print_string
    
    ; Wait for key press
    xor ah, ah
    int 0x16
    
    ; Reboot system
    mov ax, 0x0040
    mov ds, ax
    mov word [0x0072], 0x1234  ; Warm reboot flag
    jmp 0xFFFF:0x0000          ; Jump to BIOS reset vector

; ===== Data =====
msg_boot:   db 'KiraOS Stage1: Booting...', 13, 10, 0
msg_loading: db 'Loading Stage2...', 13, 10, 0
msg_ok:     db 'OK!', 13, 10, 0
msg_error:  db 'Error loading Stage2! Press any key to reboot.', 13, 10, 0

boot_drive: db BOOT_DRIVE
retry_count: db 3

; ===== Boot signature =====
times 510-($-$$) db 0
dw 0xAA55 