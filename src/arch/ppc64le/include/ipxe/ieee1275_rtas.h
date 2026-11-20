#ifndef _IPXE_IEEE1275_RTAS_H
#define _IPXE_IEEE1275_RTAS_H

/** @file
 *
 * Run-Time Abstraction Services (RTAS)
 *
 * RTAS is instantiated through the Open Firmware client interface on
 * first use, and released at shutdown.  Calls enter it through
 * ppc64le_be_call().
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>

extern int ieee1275_rtas_token ( const char *name, uint32_t *token );
extern int ieee1275_rtas_call ( uint32_t token, unsigned int nargs,
				const uint32_t *args, unsigned int nrets,
				uint32_t *rets );

#endif /* _IPXE_IEEE1275_RTAS_H */
