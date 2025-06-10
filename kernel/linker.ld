/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 16KB (0x4000) */
	    . = 0x4000;

	/* First we'll put the .text section. */
	.text ALIGN(4K) :
	{
		*(.text._start)   /* Put _start at the beginning */
		*(.text)
	}

	/* Read-only data. */
	.rodata ALIGN(4K) :
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN(4K) :
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K) :
	{
		*(COMMON)
		*(.bss)
	}

	/* Debugging information */
	/DISCARD/ : {
		*(.note*)
		*(.comment*)
		*(.eh_frame*)
		*(.debug*)
	}
} 