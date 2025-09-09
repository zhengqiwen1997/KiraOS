.section .text
.global _start

# Minimal user-mode entry: call main(), then exit(0) via int 0x80
.extern main

_start:
    # On entry, ESP points to argc laid out by the kernel (argc, argv, envp...)
    mov 4(%esp), %ebx    # EBX = argv (char**)
    mov (%esp), %eax     # EAX = argc (int)
    push %ebx            # push argv
    push %eax            # push argc
    call main
    add $8, %esp         # clean up pushed args (cdecl)
    # syscall: EXIT (0)
    xor %eax, %eax      # EAX = 0 (SystemCall::EXIT)
    xor %ebx, %ebx      # EBX = 0 (status)
    int $0x80
    hlt


