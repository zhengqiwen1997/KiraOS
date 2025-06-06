[BITS 16]
[ORG 0x7C00]

    ; Initialize segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print 'S1' for Stage1
    mov ah, 0x0E
    mov al, 'S'
    int 0x10
    mov al, '1'
    int 0x10
    mov al, ':'
    int 0x10
    mov al, ' '
    int 0x10

    ; Reset disk system
    xor ax, ax
    mov dl, 0x80    ; First hard drive
    int 0x13
    jc error

    ; Load stage2 (32 sectors = 16KB)
    mov ax, 0x07E0  ; Target segment
    mov es, ax
    xor bx, bx      ; Target offset

retry:
    mov ax, 0x0220  ; AH=02 (read), AL=32 (32 sectors)
    mov cx, 0x0002  ; CH=0 (cylinder 0), CL=2 (sector 2)
    xor dh, dh      ; Head 0
    mov dl, 0x80    ; Drive 0x80
    int 0x13
    jnc load_ok     ; If success, continue

    ; Reset and retry
    xor ax, ax
    int 0x13
    dec byte [retry_count]
    jnz retry

error:
    ; Print 'Err'
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    mov al, 'r'
    int 0x10
    mov al, 'r'
    int 0x10
    cli
    hlt

load_ok:
    ; Print 'OK'
    mov ah, 0x0E
    mov al, 'O'
    int 0x10
    mov al, 'K'
    int 0x10
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10

    ; Jump to stage2
    jmp 0x7E00

retry_count: db 3

    times 510-($-$$) db 0
    dw 0xAA55 