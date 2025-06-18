.code32

# External C++ exception handler
.extern _ZN4kira6system10Exceptions15default_handlerEPNS0_14ExceptionFrameE

.section .text

# Division error exception stub (interrupt 0)
.global exception_stub_0
exception_stub_0:
    cli                     # Disable interrupts
    pushl $0                # Push dummy error code
    pushl $0                # Push exception number
    jmp exception_common

# Debug exception stub (interrupt 1)
.global exception_stub_1
exception_stub_1:
    cli
    pushl $0
    pushl $1
    jmp exception_common

# NMI exception stub (interrupt 2)
.global exception_stub_2
exception_stub_2:
    cli
    pushl $0
    pushl $2
    jmp exception_common

# Breakpoint exception stub (interrupt 3)
.global exception_stub_3
exception_stub_3:
    cli
    pushl $0
    pushl $3
    jmp exception_common

# Overflow exception stub (interrupt 4)
.global exception_stub_4
exception_stub_4:
    cli
    pushl $0
    pushl $4
    jmp exception_common

# Bound range exception stub (interrupt 5)
.global exception_stub_5
exception_stub_5:
    cli
    pushl $0
    pushl $5
    jmp exception_common

# Invalid opcode exception stub (interrupt 6)
.global exception_stub_6
exception_stub_6:
    cli
    pushl $0
    pushl $6
    jmp exception_common

# Device not available exception stub (interrupt 7)
.global exception_stub_7
exception_stub_7:
    cli
    pushl $0
    pushl $7
    jmp exception_common

# Double fault exception stub (interrupt 8) - HAS ERROR CODE
.global exception_stub_8
exception_stub_8:
    cli
    # Error code already pushed by CPU
    pushl $8
    jmp exception_common

# Invalid TSS exception stub (interrupt 10) - HAS ERROR CODE
.global exception_stub_10
exception_stub_10:
    cli
    # Error code already pushed by CPU
    pushl $10
    jmp exception_common

# Segment not present exception stub (interrupt 11) - HAS ERROR CODE
.global exception_stub_11
exception_stub_11:
    cli
    # Error code already pushed by CPU
    pushl $11
    jmp exception_common

# Stack fault exception stub (interrupt 12) - HAS ERROR CODE
.global exception_stub_12
exception_stub_12:
    cli
    # Error code already pushed by CPU
    pushl $12
    jmp exception_common

# General protection fault exception stub (interrupt 13) - HAS ERROR CODE
.global exception_stub_13
exception_stub_13:
    cli
    # Error code already pushed by CPU
    pushl $13
    jmp exception_common

# Page fault exception stub (interrupt 14) - HAS ERROR CODE
.global exception_stub_14
exception_stub_14:
    cli
    # Error code already pushed by CPU
    pushl $14
    jmp exception_common

# Common exception handler
exception_common:
    # Save all general purpose registers
    pusha
    
    # Push pointer to exception frame (current ESP)
    pushl %esp
    
    # Call C++ exception handler
    call _ZN4kira6system10Exceptions15default_handlerEPNS0_14ExceptionFrameE
    
    # Clean up stack (remove frame pointer)
    addl $4, %esp
    
    # Restore registers
    popa
    
    # Remove exception number and error code from stack
    addl $8, %esp
    
    # Return from interrupt
    iret 