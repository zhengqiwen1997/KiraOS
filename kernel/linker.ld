OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS {
    . = 0x8000;    /* Load kernel at 32KB */

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

    /* Debugging information */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
} 