/*
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
	FILE_SECBOOT ( PERMITTED )

/** @file
 *
 * Long jumps
 *
 * The jump buffer layout matches struct jmp_buf in bits/setjmp.h:
 * r1, r2, r14-r31, LR, CR (each a doubleword).
 */

	.text

/*
 * Save stack context for non-local goto
 */
	.section ".text.setjmp", "ax", @progbits
	.globl	setjmp
	.type	setjmp, @function
setjmp:
	/* Save registers */
	std	%r1, 0(%r3)
	std	%r2, 8(%r3)
	std	%r14, 16(%r3)
	std	%r15, 24(%r3)
	std	%r16, 32(%r3)
	std	%r17, 40(%r3)
	std	%r18, 48(%r3)
	std	%r19, 56(%r3)
	std	%r20, 64(%r3)
	std	%r21, 72(%r3)
	std	%r22, 80(%r3)
	std	%r23, 88(%r3)
	std	%r24, 96(%r3)
	std	%r25, 104(%r3)
	std	%r26, 112(%r3)
	std	%r27, 120(%r3)
	std	%r28, 128(%r3)
	std	%r29, 136(%r3)
	std	%r30, 144(%r3)
	std	%r31, 152(%r3)
	mflr	%r0
	std	%r0, 160(%r3)
	mfcr	%r0
	std	%r0, 168(%r3)
	/* Return zero when returning as setjmp() */
	li	%r3, 0
	blr
	.size	setjmp, . - setjmp

/*
 * Non-local jump to a saved stack context
 */
	.section ".text.longjmp", "ax", @progbits
	.globl	longjmp
	.type	longjmp, @function
longjmp:
	/* Restore registers */
	ld	%r1, 0(%r3)
	ld	%r2, 8(%r3)
	ld	%r14, 16(%r3)
	ld	%r15, 24(%r3)
	ld	%r16, 32(%r3)
	ld	%r17, 40(%r3)
	ld	%r18, 48(%r3)
	ld	%r19, 56(%r3)
	ld	%r20, 64(%r3)
	ld	%r21, 72(%r3)
	ld	%r22, 80(%r3)
	ld	%r23, 88(%r3)
	ld	%r24, 96(%r3)
	ld	%r25, 104(%r3)
	ld	%r26, 112(%r3)
	ld	%r27, 120(%r3)
	ld	%r28, 128(%r3)
	ld	%r29, 136(%r3)
	ld	%r30, 144(%r3)
	ld	%r31, 152(%r3)
	ld	%r0, 160(%r3)
	mtlr	%r0
	ld	%r0, 168(%r3)
	mtcrf	0xff, %r0
	/* Force result to non-zero (val is an int: ignore upper 32 bits) */
	extsw.	%r3, %r4
	bnelr
	li	%r3, 1
	blr
	.size	longjmp, . - longjmp
