#ifndef _IPXE_RTAS_TIME_H
#define _IPXE_RTAS_TIME_H

/** @file
 *
 * RTAS time source
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#ifdef TIME_RTAS
#define TIME_PREFIX_rtas
#else
#define TIME_PREFIX_rtas __rtas_
#endif

#endif /* _IPXE_RTAS_TIME_H */
