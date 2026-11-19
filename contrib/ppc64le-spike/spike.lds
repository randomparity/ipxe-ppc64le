/*
 * Copyright (C) 2026 David Christensen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

/** @file
 *
 * Open Firmware spike: link script
 *
 * A single read-write-execute segment at a fixed address.
 */

OUTPUT_FORMAT ( "elf64-powerpcle" )
ENTRY ( _start )

PHDRS {
	load PT_LOAD FLAGS ( 7 ) ;
}

SECTIONS {
	. = 0x400000 ;
	.text : { *(.text) *(.text.*) } :load
	.rodata : { *(.rodata) *(.rodata.*) } :load
	.data : { *(.data) *(.data.*) *(.toc) *(.got) *(.opd) } :load
	.bss : { *(.bss) *(.bss.*) *(COMMON) } :load
	ASSERT ( _start == 0x400018, "_start must follow the 24-byte descriptor" )
	/DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) *(.gnu*) }
}
