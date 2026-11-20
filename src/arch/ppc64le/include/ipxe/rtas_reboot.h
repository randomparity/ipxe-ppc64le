#ifndef _IPXE_RTAS_REBOOT_H
#define _IPXE_RTAS_REBOOT_H

/** @file
 *
 * RTAS reboot mechanism
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#ifdef REBOOT_RTAS
#define REBOOT_PREFIX_rtas
#else
#define REBOOT_PREFIX_rtas __rtas_
#endif

#endif /* _IPXE_RTAS_REBOOT_H */
