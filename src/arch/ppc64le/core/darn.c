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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * Hardware random number generator (DARN instruction)
 *
 * The DARN instruction was introduced in Power ISA 3.0 (POWER9).  On
 * earlier processors it raises an illegal instruction exception, so
 * the processor version must be checked before the instruction is
 * first executed.  The processor version register is readable both
 * by firmware running in supervisor mode and (via kernel emulation or
 * QEMU) by a Linux user process, so no firmware call is required.
 */

#include <stdint.h>
#include <errno.h>
#include <ipxe/entropy.h>
#include <ipxe/drbg.h>

struct entropy_source darn_entropy __entropy_source ( ENTROPY_PREFERRED );

/** Number of times to retry DARN instruction */
#define DARN_RETRY_COUNT 16

/** Value returned by DARN to indicate an error */
#define DARN_ERROR 0xffffffffffffffffULL

/** Special purpose register number of the processor version register */
#define SPR_PVR 287

/** Processor version number of the first ISA 3.0 processor (POWER9) */
#define PVR_VERSION_POWER9 0x004e

/** Processor version number reported in architected compatibility mode */
#define PVR_VERSION_COMPAT 0x0f00

/** Lowest architected compatibility level that includes DARN (ISA 3.0) */
#define PVR_COMPAT_ISA_300 5

/** Colour for debug messages */
#define colour &darn_entropy

/**
 * Check for DARN support
 *
 * @v pvr		Processor version register value
 * @ret supported	DARN instruction is supported
 */
static int darn_supported ( uint32_t pvr ) {
	unsigned int version = ( pvr >> 16 );
	unsigned int revision = ( pvr & 0xffff );

	/* In architected compatibility mode, the version is fixed
	 * and the revision identifies the ISA level.
	 */
	if ( version == PVR_VERSION_COMPAT )
		return ( revision >= PVR_COMPAT_ISA_300 );

	/* Otherwise, POWER9 and all later processors support DARN */
	return ( version >= PVR_VERSION_POWER9 );
}

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int darn_entropy_enable ( void ) {
	uint64_t pvr;

	/* Check that DARN is supported, without executing it */
	__asm__ ( "mfspr %0, %1" : "=r" ( pvr ) : "i" ( SPR_PVR ) );
	if ( ! darn_supported ( pvr ) ) {
		DBGC ( colour, "DARN not supported (PVR %#08llx)\n",
		       ( ( unsigned long long ) pvr ) );
		return -ENOTSUP;
	}

	/* Data returned by DARN in conditioned mode is full entropy,
	 * up to a security strength of 128 bits, so assume that each
	 * sample contains exactly 8 bits of entropy.
	 */
	if ( DRBG_SECURITY_STRENGTH > 128 )
		return -ENOTSUP;
	entropy_init ( &darn_entropy, MIN_ENTROPY ( 8.0 ) );

	return 0;
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int darn_get_noise ( noise_sample_t *noise ) {
	uint64_t result;
	unsigned int i;

	/* Issue DARN (64-bit conditioned), retrying on error */
	for ( i = 0 ; i < DARN_RETRY_COUNT ; i++ ) {
		__asm__ __volatile__ ( ".machine push\n\t"
				       ".machine \"power9\"\n\t"
				       "darn %0, 1\n\t"
				       ".machine pop\n\t"
				       : "=r" ( result ) );
		if ( result != DARN_ERROR ) {
			*noise = result;
			return 0;
		}
	}

	DBGC ( colour, "DARN failed to return a random number\n" );
	return -EBUSY;
}

/** Hardware random number generator entropy source */
struct entropy_source darn_entropy __entropy_source ( ENTROPY_PREFERRED ) = {
	.name = "darn",
	.enable = darn_entropy_enable,
	.get_noise = darn_get_noise,
};
