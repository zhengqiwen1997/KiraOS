[BITS 32]
[ORG 0x8000]

; Ultra simple test kernel for KiraOS
; This kernel fills the entire screen with a very obvious pattern

_start:
    ; VERY FIRST THING: Send a unique pattern to the serial port
    mov dx, 0x3F8          ; Serial port COM1
    mov al, '!'
    out dx, al
    mov al, '!'
    out dx, al
    mov al, '!'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'K'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'N'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'L'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'X'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'C'
    out dx, al
    mov al, 'U'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, 'I'
    out dx, al
    mov al, 'N'
    out dx, al
    mov al, 'G'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'A'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, '0'
    out dx, al
    mov al, 'x'
    out dx, al
    mov al, '8'
    out dx, al
    mov al, '0'
    out dx, al
    mov al, '0'
    out dx, al
    mov al, '0'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, '!'
    out dx, al
    mov al, '!'
    out dx, al
    mov al, '!'
    out dx, al
    mov al, 13             ; Carriage return
    out dx, al
    mov al, 10             ; Line feed
    out dx, al

    ; First, output to serial port to confirm we're in the kernel
    mov dx, 0x3F8          ; Serial port COM1
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'K'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'N'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'L'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'S'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, 'A'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'D'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, 13             ; Carriage return
    out dx, al
    mov al, 10             ; Line feed
    out dx, al

    ; Fill the entire screen with a very obvious pattern - all bright white on bright red
    mov edi, 0xB8000       ; Video memory base address
    mov ecx, 80*25         ; 80 columns * 25 rows = 2000 characters
    mov ax, 0xCFDB         ; Full block character with bright white on bright red
    rep stosw              ; Repeat store word ECX times

    ; Write "KERNEL RUNNING!" to the center of the screen in large letters
    mov edi, 0xB8000 + (12 * 80 + 30) * 2  ; Row 12, Column 30
    
    ; "KERNEL RUNNING!"
    mov ah, 0x1F           ; Blue on white (very visible)
    
    ; 'K'
    mov al, 'K'
    mov [edi], ax
    add edi, 2
    
    ; 'E'
    mov al, 'E'
    mov [edi], ax
    add edi, 2
    
    ; 'R'
    mov al, 'R'
    mov [edi], ax
    add edi, 2
    
    ; 'N'
    mov al, 'N'
    mov [edi], ax
    add edi, 2
    
    ; 'E'
    mov al, 'E'
    mov [edi], ax
    add edi, 2
    
    ; 'L'
    mov al, 'L'
    mov [edi], ax
    add edi, 2
    
    ; ' '
    mov al, ' '
    mov [edi], ax
    add edi, 2
    
    ; 'R'
    mov al, 'R'
    mov [edi], ax
    add edi, 2
    
    ; 'U'
    mov al, 'U'
    mov [edi], ax
    add edi, 2
    
    ; 'N'
    mov al, 'N'
    mov [edi], ax
    add edi, 2
    
    ; 'N'
    mov al, 'N'
    mov [edi], ax
    add edi, 2
    
    ; 'I'
    mov al, 'I'
    mov [edi], ax
    add edi, 2
    
    ; 'N'
    mov al, 'N'
    mov [edi], ax
    add edi, 2
    
    ; 'G'
    mov al, 'G'
    mov [edi], ax
    add edi, 2
    
    ; '!'
    mov al, '!'
    mov [edi], ax
    add edi, 2

    ; Output completion message to serial port
    mov dx, 0x3F8          ; Serial port COM1
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'K'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'N'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'L'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, 'C'
    out dx, al
    mov al, 'O'
    out dx, al
    mov al, 'M'
    out dx, al
    mov al, 'P'
    out dx, al
    mov al, 'L'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, ' '
    out dx, al
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, '*'
    out dx, al
    mov al, 13             ; Carriage return
    out dx, al
    mov al, 10             ; Line feed
    out dx, al

    ; Infinite loop to keep the kernel running
    jmp $ 