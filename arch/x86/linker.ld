/* Obsidian Kernel i686 linker script.
   Everything loads at 1 MiB; the Multiboot header section leads the
   image so GRUB finds it immediately. _kernel_end marks the first free
   byte after the image for the physical memory manager and heap. */

ENTRY(_start)

SECTIONS
{
	. = 1M;

	.text BLOCK(4K) : ALIGN(4K)
	{
		KEEP(*(.multiboot))
		*(.text .text.*)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata .rodata.*)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data .data.*)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss .bss.*)
		*(.stack)
	}

	_kernel_end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
	}
}
