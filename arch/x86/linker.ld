ENTRY (_start)

SECTIONS
{
	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* Note that we page-align the sections. */
	. = 0xC0100000;
	/* Add a symbol that indicates the start address of the kernel. */
	_kernel_start = .;
	_text_start = .;
	.text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.multiboot)
		*(.text)
	}
	_text_end = .;
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata)
		*(.rodata.*)
	}
	.data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)

		__kernel_map_start = .;
		KEEP(*(.kernel_map))
		__kernel_map_end = .;
	}
    _bss_start = .;
	.bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}
    _bss_end = .;
	/* Add a symbol that indicates the end address of the kernel. */
	_kernel_end = .;
}
