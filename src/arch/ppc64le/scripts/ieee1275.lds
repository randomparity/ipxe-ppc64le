/*
 * Linker script for Open Firmware (ieee1275) images
 *
 * The firmware loads each segment of the ELF image at its physical
 * address, with the MMU disabled.  The image is therefore linked as a
 * single read-write-execute segment at a fixed address, and needs no
 * relocation.  This script, together with the prefix, is the only
 * place that determines the ELF program headers and entry point.
 *
 */

OUTPUT_FORMAT ( "elf64-powerpcle" )
ENTRY ( _elf_start )

/* Fixed load address (above the firmware's low memory) */
_base = 0x400000 ;

PHDRS {
    load PT_LOAD FLAGS ( 7 ) ;
}

SECTIONS {

    /* Weak symbols that need zero values if not otherwise defined */
    saved_pos = .;
    .weak 0x0 : {
	_weak = .;
	*(.weak)
	*(.weak.*)
	_eweak = .;
    }
    _assert = ASSERT ( ( _weak == _eweak ), ".weak is non-zero length" );
    _assert = ASSERT ( ( . == saved_pos ), ".weak altered current position" );

    /* Fixed load address */
    . = _base ;

    /* Prefix code */
    .prefix : {
	_prefix = .;
	*(.prefix)
	*(.prefix.*)
	_eprefix = .;
    } :load

    /* Program code */
    .text : {
	_text = .;
	*(.text)
	*(.text.*)
	*(.glink)
	*(.iplt)
	_etext = .;
    } :load

    /* Read-only data */
    .rodata : {
	_rodata = .;
	*(.rodata)
	*(.rodata.*)
	_erodata = .;
    } :load

    /* Table of contents (addressed relative to r2 = .TOC.) */
    .got : {
	*(.got)
	*(.toc)
	*(.branch_lt)
    } :load

    /* Writable data */
    .data : {
	_data = .;
	*(.data)
	*(.data.*)
	KEEP(*(SORT(.tbl.*)))	/* Various tables.  See include/tables.h */
	KEEP(*(.provided))
	KEEP(*(.provided.*))
	_edata = .;
    } :load

    /* Uninitialised data */
    .bss : {
	_bss = .;
	*(.sbss)
	*(.sbss.*)
	*(.bss)
	*(.bss.*)
	*(COMMON)
	. = ALIGN ( 16 );
	_ebss = .;
    } :load

    /* Stack (uninitialised, not zeroed) */
    .stack : {
	*(.stack)
	*(.stack.*)
    } :load

    /* End virtual address */
    _end = .;

    /* Firmware call arguments must be addressable in 32 bits */
    _assert = ASSERT ( ( _end < 0x100000000 ), "image exceeds 4GiB" );

    /* Unwanted sections */
    /DISCARD/ : {
	*(.comment)
	*(.comment.*)
	*(.note)
	*(.note.*)
	*(.eh_frame)
	*(.eh_frame.*)
	*(.dynamic)
	*(.dynsym)
	*(.dynstr)
	*(.hash)
	*(.gnu.hash)
	*(.einfo)
	*(.einfo.*)
	*(.discard)
	*(.discard.*)
	*(.pci_devlist.*)
    }
}
