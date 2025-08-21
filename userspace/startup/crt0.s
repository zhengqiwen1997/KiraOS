.section .text
.global _start

# Minimal user-mode entry: call main(), then exit(0) via int 0x80
.extern main

_start:
    call main
    # syscall: EXIT (0)
    xor %eax, %eax      # EAX = 0 (SystemCall::EXIT)
    xor %ebx, %ebx      # EBX = 0 (status)
    int $0x80
    hlt


