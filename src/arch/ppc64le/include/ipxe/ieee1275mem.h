#ifndef _IPXE_IEEE1275MEM_H
#define _IPXE_IEEE1275MEM_H

/** @file
 *
 * Open Firmware memory map
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#ifdef MEMMAP_IEEE1275
#define MEMMAP_PREFIX_ieee1275
#else
#define MEMMAP_PREFIX_ieee1275 __ieee1275_
#endif

/**
 * Synchronise in-use regions with the externally visible system memory map
 *
 */
static inline __attribute__ (( always_inline )) void
MEMMAP_INLINE ( ieee1275, memmap_sync ) ( void ) {
	/* Nothing to do */
}

#endif /* _IPXE_IEEE1275MEM_H */
