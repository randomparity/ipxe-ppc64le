#ifndef _IPXE_TIMEBASE_H
#define _IPXE_TIMEBASE_H

/** @file
 *
 * PowerPC timebase
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>

extern int timebase_frequency ( uint64_t *frequency );

#endif /* _IPXE_TIMEBASE_H */
