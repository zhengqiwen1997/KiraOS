.section .text
.global _start

# Minimal user-mode entry: call user_entry(), then exit(0) via int 0x80
.extern user_entry

_start:
    call user_entry
    # syscall: EXIT (0)
    xor %eax, %eax      # EAX = 0 (SystemCall::EXIT)
    xor %ebx, %ebx      # EBX = 0 (status)
    int $0x80
    hlt


