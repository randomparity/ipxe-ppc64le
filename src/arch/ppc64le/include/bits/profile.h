#ifndef _BITS_PROFILE_H
#define _BITS_PROFILE_H

/** @file
 *
 * Profiling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>

/**
 * Get profiling timestamp
 *
 * @ret timestamp	Timestamp
 */
static inline __attribute__ (( always_inline )) unsigned long
profile_timestamp ( void ) {
	uint64_t ticks;

	/* Read time base register */
	__asm__ __volatile__ ( "mftb %0" : "=r" ( ticks ) );
	return ticks;
}

#endif /* _BITS_PROFILE_H */
