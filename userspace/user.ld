/* Linker script for KiraOS user programs */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS
{
    /* Standard ELF text start address */
    . = 0x08048000;

    /* Code section */
    .text ALIGN(4K) :
    {
        *(.text._start)   /* Put _start at the beginning */
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }

    /* Initialized data */
    .data ALIGN(4K) :
    {
        *(.data)
    }

    /* Uninitialized data (BSS) */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }

    /* Discard debugging information */
    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
        *(.debug*)
    }
} 