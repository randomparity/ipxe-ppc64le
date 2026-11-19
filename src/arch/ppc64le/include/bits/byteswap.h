#ifndef _BITS_BYTESWAP_H
#define _BITS_BYTESWAP_H

/** @file
 *
 * Byte-order swapping functions
 *
 */

#include <stdint.h>

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

static inline __attribute__ (( always_inline, const )) uint16_t
__bswap_variable_16 ( uint16_t x ) {
	return __builtin_bswap16 ( x );
}

static inline __attribute__ (( always_inline )) void
__bswap_16s ( uint16_t *x ) {
	*x = __builtin_bswap16 ( *x );
}

static inline __attribute__ (( always_inline, const )) uint32_t
__bswap_variable_32 ( uint32_t x ) {
	return __builtin_bswap32 ( x );
}

static inline __attribute__ (( always_inline )) void
__bswap_32s ( uint32_t *x ) {
	*x = __builtin_bswap32 ( *x );
}

static inline __attribute__ (( always_inline, const )) uint64_t
__bswap_variable_64 ( uint64_t x ) {
	return __builtin_bswap64 ( x );
}

static inline __attribute__ (( always_inline )) void
__bswap_64s ( uint64_t *x ) {
	*x = __builtin_bswap64 ( *x );
}

#endif /* _BITS_BYTESWAP_H */
