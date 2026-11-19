#ifndef _BITS_BIGINT_H
#define _BITS_BIGINT_H

/** @file
 *
 * Big integer support
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>

/** Element of a big integer */
typedef uint64_t bigint_element_t;

/**
 * Add big integers
 *
 * @v addend0		Element 0 of big integer to add
 * @v value0		Element 0 of big integer to be added to
 * @v size		Number of elements
 * @ret carry		Carry out
 */
static inline __attribute__ (( always_inline )) int
bigint_add_raw ( const uint64_t *addend0, uint64_t *value0,
		 unsigned int size ) {
	uint64_t carry = 0;
	uint64_t addend;
	uint64_t value;
	int carry_in;
	int carry_out;
	unsigned int i;

	/* Operands may alias: read each addend element before writing */
	for ( i = 0 ; i < size ; i++ ) {
		addend = addend0[i];
		value = value0[i];
		carry_in = __builtin_add_overflow ( value, carry, &value );
		carry_out = __builtin_add_overflow ( value, addend, &value );
		value0[i] = value;
		carry = ( carry_in | carry_out );
	}
	return carry;
}

/**
 * Subtract big integers
 *
 * @v subtrahend0	Element 0 of big integer to subtract
 * @v value0		Element 0 of big integer to be subtracted from
 * @v size		Number of elements
 * @ret borrow		Borrow out
 */
static inline __attribute__ (( always_inline )) int
bigint_subtract_raw ( const uint64_t *subtrahend0, uint64_t *value0,
		      unsigned int size ) {
	uint64_t borrow = 0;
	uint64_t subtrahend;
	uint64_t value;
	int borrow_in;
	int borrow_out;
	unsigned int i;

	/* Operands may alias: read each subtrahend element before writing */
	for ( i = 0 ; i < size ; i++ ) {
		subtrahend = subtrahend0[i];
		value = value0[i];
		borrow_in = __builtin_sub_overflow ( value, borrow, &value );
		borrow_out = __builtin_sub_overflow ( value, subtrahend,
						      &value );
		value0[i] = value;
		borrow = ( borrow_in | borrow_out );
	}
	return borrow;
}

/**
 * Shift big integer left
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret out		Bit shifted out
 */
static inline __attribute__ (( always_inline )) int
bigint_shl_raw ( uint64_t *value0, unsigned int size ) {
	uint64_t carry = 0;
	uint64_t out;
	unsigned int i;

	for ( i = 0 ; i < size ; i++ ) {
		out = ( value0[i] >> 63 );
		value0[i] = ( ( value0[i] << 1 ) | carry );
		carry = out;
	}
	return carry;
}

/**
 * Shift big integer right
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret out		Bit shifted out
 */
static inline __attribute__ (( always_inline )) int
bigint_shr_raw ( uint64_t *value0, unsigned int size ) {
	uint64_t carry = 0;
	uint64_t out;
	unsigned int i;

	for ( i = size ; i-- ; ) {
		out = ( value0[i] & 1 );
		value0[i] = ( ( value0[i] >> 1 ) | ( carry << 63 ) );
		carry = out;
	}
	return carry;
}

/**
 * Grow big integer
 *
 * @v source0		Element 0 of source big integer
 * @v source_size	Number of elements in source big integer
 * @v dest0		Element 0 of destination big integer
 * @v dest_size		Number of elements in destination big integer
 */
static inline __attribute__ (( always_inline )) void
bigint_grow_raw ( const uint64_t *source0, unsigned int source_size,
		  uint64_t *dest0, unsigned int dest_size ) {
	unsigned int i;

	for ( i = 0 ; i < source_size ; i++ )
		dest0[i] = source0[i];
	for ( ; i < dest_size ; i++ )
		dest0[i] = 0;
}

/**
 * Shrink big integer
 *
 * @v source0		Element 0 of source big integer
 * @v source_size	Number of elements in source big integer
 * @v dest0		Element 0 of destination big integer
 * @v dest_size		Number of elements in destination big integer
 */
static inline __attribute__ (( always_inline )) void
bigint_shrink_raw ( const uint64_t *source0, unsigned int source_size __unused,
		    uint64_t *dest0, unsigned int dest_size ) {
	unsigned int i;

	for ( i = 0 ; i < dest_size ; i++ )
		dest0[i] = source0[i];
}

/**
 * Multiply big integer elements
 *
 * @v multiplicand	Multiplicand element
 * @v multiplier	Multiplier element
 * @v result		Result element
 * @v carry		Carry element
 */
static inline __attribute__ (( always_inline )) void
bigint_multiply_one ( const uint64_t multiplicand, const uint64_t multiplier,
		      uint64_t *result, uint64_t *carry ) {
	unsigned __int128 product;

	/* Cannot overflow: (2^64-1)^2 + 2*(2^64-1) == 2^128-1 */
	product = ( ( ( unsigned __int128 ) multiplicand * multiplier ) +
		    *result + *carry );
	*result = product;
	*carry = ( product >> 64 );
}

#endif /* _BITS_BIGINT_H */
