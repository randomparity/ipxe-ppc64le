#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** A jump buffer */
typedef struct {
	/** Stack pointer (r1) */
	unsigned long sp;
	/** TOC pointer (r2) */
	unsigned long toc;
	/** Callee-saved general-purpose registers (r14-r31) */
	unsigned long gprs[18];
	/** Link register */
	unsigned long lr;
	/** Condition register */
	unsigned long cr;
} jmp_buf[1];

#endif /* _BITS_SETJMP_H */
