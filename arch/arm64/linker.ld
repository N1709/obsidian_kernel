/* Obsidian Kernel ARM64 linker script.
 *
 * QEMU virt and U-Boot load the image at 0x40080000; the DTB sits
 * elsewhere in RAM so nothing collides. __stack_top reserves 64 KiB
 * of stack inside BSS where start.S parks the stack pointer.
 */

ENTRY(_start)

SECTIONS
{
	. = 0x40080000;

	__image_start = .;

	.text : ALIGN(4096)
	{
		KEEP(*(.text.boot))
		*(.text.boot.entry)
		*(.text .text.*)
	}

	.rodata : ALIGN(4096)
	{
		*(.rodata .rodata.*)
	}

	.data : ALIGN(4096)
	{
		*(.data .data.*)
	}

	. = ALIGN(16);
	__bss_start = .;
	.bss (NOLOAD) : ALIGN(16)
	{
		*(.bss .bss.*)
		*(COMMON)

		. = ALIGN(16);
		. += 65536;
		__stack_top = .;
	}
	__bss_end = .;

	__image_end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
	}
}
