#ifndef _BITS_BITOPS_H
#define _BITS_BITOPS_H

/** @file
 *
 * PowerPC64 bit operations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>

/**
 * Test and set bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 * @ret old		Old value of bit (zero or non-zero)
 */
static inline __attribute__ (( always_inline )) int
test_and_set_bit ( unsigned int bit, volatile void *bits ) {
	unsigned int index = ( bit / 64 );
	unsigned int offset = ( bit % 64 );
	volatile uint64_t *qword = ( ( ( volatile uint64_t * ) bits ) + index );
	uint64_t mask = ( 1UL << offset );
	uint64_t old;

	old = __atomic_fetch_or ( qword, mask, __ATOMIC_SEQ_CST );
	return ( !! ( old & mask ) );
}

/**
 * Test and clear bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 * @ret old		Old value of bit (zero or non-zero)
 */
static inline __attribute__ (( always_inline )) int
test_and_clear_bit ( unsigned int bit, volatile void *bits ) {
	unsigned int index = ( bit / 64 );
	unsigned int offset = ( bit % 64 );
	volatile uint64_t *qword = ( ( ( volatile uint64_t * ) bits ) + index );
	uint64_t mask = ( 1UL << offset );
	uint64_t old;

	old = __atomic_fetch_and ( qword, ~mask, __ATOMIC_SEQ_CST );
	return ( !! ( old & mask ) );
}

/**
 * Set bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 */
static inline __attribute__ (( always_inline )) void
set_bit ( unsigned int bit, volatile void *bits ) {

	test_and_set_bit ( bit, bits );
}

/**
 * Clear bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 */
static inline __attribute__ (( always_inline )) void
clear_bit ( unsigned int bit, volatile void *bits ) {

	test_and_clear_bit ( bit, bits );
}

#endif /* _BITS_BITOPS_H */
