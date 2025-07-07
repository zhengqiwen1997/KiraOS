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

# x87 FPU floating-point error exception stub (interrupt 16)
.global exception_stub_16
exception_stub_16:
    cli
    pushl $0                # Push dummy error code (no error code for INT 16)
    pushl $16               # Push exception number
    jmp exception_common

# SIMD floating-point exception stub (interrupt 19)
.global exception_stub_19
exception_stub_19:
    cli
    pushl $0                # Push dummy error code (no error code for INT 19)
    pushl $19               # Push exception number
    jmp exception_common

# Hardware interrupt stubs (IRQ 0-15 -> INT 32-47)

# External C-style hardware interrupt handler wrapper
.extern irq_default_handler_wrapper

# Timer interrupt stub (IRQ 0 -> INT 32)
.global irq_stub_0
irq_stub_0:
    cli
    pushl $0                # Push dummy error code
    pushl $32               # Push interrupt number
    jmp irq_common

# Keyboard interrupt stub (IRQ 1 -> INT 33)
.global irq_stub_1
irq_stub_1:
    cli
    pushl $0
    pushl $33
    jmp irq_common

# IRQ 2-15 stubs (cascade, COM, LPT, etc.)
.global irq_stub_2
irq_stub_2:
    cli
    pushl $0
    pushl $34
    jmp irq_common

.global irq_stub_3
irq_stub_3:
    cli
    pushl $0
    pushl $35
    jmp irq_common

.global irq_stub_4
irq_stub_4:
    cli
    pushl $0
    pushl $36
    jmp irq_common

.global irq_stub_5
irq_stub_5:
    cli
    pushl $0
    pushl $37
    jmp irq_common

.global irq_stub_6
irq_stub_6:
    cli
    pushl $0
    pushl $38
    jmp irq_common

.global irq_stub_7
irq_stub_7:
    cli
    pushl $0
    pushl $39
    jmp irq_common

.global irq_stub_8
irq_stub_8:
    cli
    pushl $0
    pushl $40
    jmp irq_common

.global irq_stub_9
irq_stub_9:
    cli
    pushl $0
    pushl $41
    jmp irq_common

.global irq_stub_10
irq_stub_10:
    cli
    pushl $0
    pushl $42
    jmp irq_common

.global irq_stub_11
irq_stub_11:
    cli
    pushl $0
    pushl $43
    jmp irq_common

.global irq_stub_12
irq_stub_12:
    cli
    pushl $0
    pushl $44
    jmp irq_common

.global irq_stub_13
irq_stub_13:
    cli
    pushl $0
    pushl $45
    jmp irq_common

.global irq_stub_14
irq_stub_14:
    cli
    pushl $0
    pushl $46
    jmp irq_common

.global irq_stub_15
irq_stub_15:
    cli
    pushl $0
    pushl $47
    jmp irq_common

# Common hardware interrupt handler
irq_common:
    # Save all general purpose registers
    pusha
    
    # Push pointer to IRQ frame (current ESP)
    pushl %esp
    
    # Call C-style IRQ handler wrapper
    call irq_default_handler_wrapper
    
    # Clean up stack (remove frame pointer)
    addl $4, %esp
    
    # Restore registers
    popa
    
    # Remove interrupt number and error code from stack
    addl $8, %esp
    
    # Return from interrupt
    iret

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
