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


	FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )
	FILE_SECBOOT ( FORBIDDEN )

/** @file
 *
 * Open Firmware executable prefix
 *
 * The firmware loads the ELF image at its physical address and enters
 * it with the MMU disabled, in either endianness, with the client
 * interface entry point in %r5.  This prefix and the linker script
 * are the only places that know about the ELF entry convention.
 *
 * The first instructions are an endianness polyglot: in little-endian
 * mode the first word is a no-op and the second a branch past the
 * trampoline; in big-endian mode the first word decodes as a branch
 * over the second, and the trampoline switches to little-endian with
 * rfid.  The trampoline words are big-endian encodings, written as
 * little-endian .long values so that they assemble to the right bytes.
 */

/** Stack size */
#define STACK_SIZE 0x10000

	.section ".note.GNU-stack", "", @progbits
	.section ".prefix", "ax", @progbits
	.globl	_elf_start
_elf_start:
	tdi	0, 0, 0x48		/* BE: "b .+8"; LE: no-op */
	b	le_entry		/* LE: skip trampoline; BE: skipped */
	.long	0xa600607d		/* mfmsr r11 */
	.long	0x01006b69		/* xori r11,r11,1 */
	.long	0x00004039		/* li r10,0 */
	.long	0x6401417d		/* mtmsrd r10,1 (clear EE, RI) */
	.long	0x05009f42		/* bcl 20,31,.+4 */
	.long	0xa602487d		/* mflr r10 */
	.long	0x14004a39		/* addi r10,r10,20 */
	.long	0xa6035a7d		/* mtsrr0 r10 */
	.long	0xa6037b7d		/* mtsrr1 r11 */
	.long	0x2400004c		/* rfid */
le_entry:

	/* Preserve the client interface entry point across startup */
	mr	%r31, %r5

	/* Establish table of contents and stack */
	lis	%r2, .TOC.@ha
	addi	%r2, %r2, .TOC.@l
	lis	%r1, _estack@ha
	addi	%r1, %r1, _estack@l
	li	%r0, 0
	stdu	%r0, -32(%r1)

	/* Zero the uninitialised data */
	lis	%r9, _bss@ha
	addi	%r9, %r9, _bss@l
	lis	%r10, _ebss@ha
	addi	%r10, %r10, _ebss@l
1:	std	%r0, 0(%r9)
	addi	%r9, %r9, 8
	cmpld	%r9, %r10
	blt	1b

	/* Record the client interface entry point */
	lis	%r9, ieee1275_entry@ha
	std	%r31, ieee1275_entry@l(%r9)

	/* Call main program, then return to the firmware */
	bl	main
	nop
	bl	ieee1275_exit
	nop
	b	.
	.size	_elf_start, . - _elf_start

	/* Stack */
	.section ".stack", "aw", @nobits
	.balign	16
	.space	STACK_SIZE
	.globl	_estack
_estack:
