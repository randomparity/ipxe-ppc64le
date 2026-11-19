#ifndef _IPXE_IEEE1275_DHCPARCH_H
#define _IPXE_IEEE1275_DHCPARCH_H

/** @file
 *
 * DHCP client architecture definitions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/*
 * There is no specification-defined DHCP architecture for PXE clients
 * running under Open Firmware on PowerPC.  Use the value that is
 * already defined for this architecture's EFI clients.
 *
 */
#include <ipxe/efi/dhcparch.h>

#endif /* _IPXE_IEEE1275_DHCPARCH_H */
