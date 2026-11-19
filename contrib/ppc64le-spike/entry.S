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
 * Open Firmware spike: entry stub and client-interface call thunk
 *
 * The firmware may enter a client program in either endianness, so the
 * first instructions are a polyglot: in little-endian mode the first
 * word is a no-op trap-doubleword and the second a branch past the
 * trampoline; in big-endian mode the first word decodes as a branch
 * over the second, and the trampoline switches to little-endian with
 * rfid.  The trampoline words are big-endian encodings, written as
 * little-endian .long values so that they assemble to the right bytes.
 *
 * Both the Open Firmware client interface and RTAS are big-endian, so
 * be_call() switches MSR[LE] off with rfid around each call and
 * switches it back on return (the thunk owns the mode switch).
 */

#define STACK_SIZE 0x8000
#define FRAME 320
#define SAVE ( 160 )

	.text
	.globl	_start
	/*
	 * Function descriptor, used only by the ELF variants whose entry
	 * point is a descriptor (see mkvariants.py); patched in the file
	 */
_opd:
	.space	24
_start:
	/* Endianness polyglot (see above) */
	tdi	0, 0, 0x48		/* BE: "b .+8"; LE: no-op */
	b	le_direct		/* LE: skip trampoline; BE: skipped */
	.long	0xa600607d		/* mfmsr r11 */
	.long	0x01006b69		/* xori r11,r11,1 */
	.long	0x01008039		/* li r12,1 (marks a BE entry) */
	.long	0x00004039		/* li r10,0 */
	.long	0x6401417d		/* mtmsrd r10,1 (clear EE, RI) */
	.long	0x05009f42		/* bcl 20,31,.+4 */
	.long	0xa602487d		/* mflr r10 */
	.long	0x14004a39		/* addi r10,r10,20 */
	.long	0xa6035a7d		/* mtsrr0 r10 */
	.long	0xa6037b7d		/* mtsrr1 r11 */
	.long	0x2400004c		/* rfid */
	li	r9, 0			/* rfid lands here (BE entry only) */
	b	le_common
le_direct:
	mfmsr	r11
	mr	r9, r12			/* firmware's r12 (entry address) */
	li	r12, 0
le_common:
	/* r3-r7: firmware arguments; r11: MSR (toggled if r12 is 1) */
	xor	r11, r11, r12
	lis	r8, entry_state@ha
	addi	r8, r8, entry_state@l
	std	r1, 0(r8)
	std	r3, 8(r8)
	std	r4, 16(r8)
	std	r5, 24(r8)
	std	r6, 32(r8)
	std	r7, 40(r8)
	std	r11, 48(r8)		/* MSR at entry */
	std	r12, 56(r8)		/* 1 if entered big-endian */
	std	r9, 96(r8)		/* r12 at entry (LE direct entry only) */
	std	r13, 104(r8)
	bl	1f
1:	mflr	r9
	std	r9, 64(r8)		/* run-time address of this point */
	mfspr	r9, 287			/* PVR */
	std	r9, 72(r8)
	std	r2, 80(r8)
	lis	r9, 1b@ha
	addi	r9, r9, 1b@l
	std	r9, 88(r8)

	/* Establish TOC and stack, then enter C */
	lis	r2, .TOC.@ha
	addi	r2, r2, .TOC.@l
	lis	r1, stack_top@ha
	addi	r1, r1, stack_top@l
	li	r0, 0
	std	r0, 0(r1)
	bl	probe_main
	nop
	b	.
	.size	_start, . - _start

	/*
	 * uint64_t be_call ( entry, a0, a1, a2 )
	 *
	 * Call big-endian code at entry with r3=a0, r4=a1, r5=a2
	 */
	.globl	be_call
	.type	be_call, @function
be_call:
	mflr	r0
	std	r0, 16(r1)
	stdu	r1, -FRAME(r1)
	std	r2, SAVE(r1)
	std	r13, SAVE+8(r1)
	.set	off, SAVE+16
	.irp	reg, 14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	std	\reg, off(r1)
	.set	off, off+8
	.endr
	mr	r12, r3
	mr	r3, r4
	mr	r4, r5
	mr	r5, r6
	mfmsr	r30			/* little-endian MSR, for the return */
	bcl	20, 31, 0f
0:	mflr	r10
	addi	r31, r10, ( be_return_cont - 0b )
	addi	r10, r10, ( be_return - 0b )
	mtlr	r10			/* callee returns into BE stub */
	li	r9, 1
	andc	r9, r30, r9		/* MSR with LE clear */
	mtsrr0	r12
	mtsrr1	r9
	rfid
	.balign	4
be_return:
	.long	0xa603fa7f		/* mtsrr0 r31 */
	.long	0xa603db7f		/* mtsrr1 r30 */
	.long	0x2400004c		/* rfid */
be_return_cont:
	.set	off, SAVE+16
	.irp	reg, 14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	ld	\reg, off(r1)
	.set	off, off+8
	.endr
	ld	r13, SAVE+8(r1)
	ld	r2, SAVE(r1)
	addi	r1, r1, FRAME
	ld	r0, 16(r1)
	mtlr	r0
	blr
	.size	be_call, . - be_call

	.bss
	.balign	16
entry_state:
	.space	128
	.globl	entry_state
	.space	STACK_SIZE
stack_top:
	.section ".note.GNU-stack", "", @progbits
