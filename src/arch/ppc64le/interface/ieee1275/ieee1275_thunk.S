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
 * Open Firmware call thunk
 *
 * The Open Firmware client interface (and RTAS) are big-endian, while
 * iPXE runs little-endian.  The thunk owns the mode switch: it clears
 * MSR[LE] with rfid around each call and restores the little-endian
 * MSR on return.  It is the only code that changes the MSR after
 * entry; byte-swapping of the argument cells is done by
 * ieee1275_call().
 */

/** Stack frame size */
#define FRAME 320

/** Offset of register save area within the frame */
#define SAVE 160

	.section ".note.GNU-stack", "", @progbits
	.text

	/**
	 * Call big-endian firmware code
	 *
	 * @v r3		Entry point
	 * @v r4		First argument (%r3 for the callee)
	 * @v r5		Second argument (%r4 for the callee)
	 * @v r6		Third argument (%r5 for the callee)
	 * @ret r3		Value returned by the callee
	 */
	.globl	ppc64le_be_call
	.type	ppc64le_be_call, @function
ppc64le_be_call:
	mflr	%r0
	std	%r0, 16(%r1)
	stdu	%r1, -FRAME(%r1)
	std	%r2, SAVE(%r1)
	std	%r13, SAVE+8(%r1)
	.set	off, SAVE+16
	.irp	reg, 14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	std	%r\reg, off(%r1)
	.set	off, off+8
	.endr

	/* Shuffle arguments and prepare the return path */
	mr	%r12, %r3
	mr	%r3, %r4
	mr	%r4, %r5
	mr	%r5, %r6
	mfmsr	%r30			/* little-endian MSR, for the return */
	bcl	20, 31, 0f
0:	mflr	%r10
	addi	%r31, %r10, ( be_return_cont - 0b )
	addi	%r10, %r10, ( be_return - 0b )
	mtlr	%r10			/* callee returns into BE stub */

	/* Enter the callee with MSR[LE] clear */
	li	%r9, 1
	andc	%r9, %r30, %r9
	mtsrr0	%r12
	mtsrr1	%r9
	rfid

	/* Big-endian return stub: restore the little-endian MSR */
	.balign	4
be_return:
	.long	0xa603fa7f		/* mtsrr0 r31 */
	.long	0xa603db7f		/* mtsrr1 r30 */
	.long	0x2400004c		/* rfid */

	/* Little-endian continuation */
be_return_cont:
	.set	off, SAVE+16
	.irp	reg, 14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	ld	%r\reg, off(%r1)
	.set	off, off+8
	.endr
	ld	%r13, SAVE+8(%r1)
	ld	%r2, SAVE(%r1)
	addi	%r1, %r1, FRAME
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr
	.size	ppc64le_be_call, . - ppc64le_be_call
