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
FILE_SECBOOT ( FORBIDDEN );

/** @file
 *
 * Open Firmware kernel boot primitives
 *
 * A Linux kernel started by Open Firmware reads its command line from
 * the @c bootargs property of the @c /chosen node, is given its initrd
 * in r3 and r4, and is given the client interface entry point in r5.
 * It calls the firmware for memory of its own, so memory that iPXE
 * holds and no longer needs is released first.
 */

#include <string.h>
#include <errno.h>
#include <ipxe/ieee1275.h>
#include <ipxe/ppc64le_kernel.h>

/** Maximum length of the command line, including the terminator */
#define IEEE1275_BOOTARGS_MAX 4096

/**
 * Claim memory for a kernel
 *
 * @v size		Size of kernel
 * @v base		Base address to fill in
 * @ret rc		Return status code
 */
int ppc64le_boot_claim ( size_t size, uint64_t *base ) {

	return ieee1275_memmap_claim_kernel ( size, base );
}

/**
 * Release memory claimed for a kernel
 *
 */
void ppc64le_boot_unclaim ( void ) {

	ieee1275_memmap_release_kernel();
}

/**
 * Prepare the firmware for a kernel
 *
 * @v cmdline		Kernel command line, or NULL
 * @ret rc		Return status code
 *
 * Failure leaves iPXE usable, so this must be called before shutdown.
 */
int ppc64le_boot_prepare ( const char *cmdline ) {
	ieee1275_phandle_t chosen;
	size_t len;
	int rc;

	/* Construct the property value, with its terminator */
	if ( ! cmdline )
		cmdline = "";
	len = ( strlen ( cmdline ) + 1 );
	if ( len > IEEE1275_BOOTARGS_MAX ) {
		DBGC ( &ieee1275_entry, "IEEE1275 command line too long\n" );
		return -E2BIG;
	}

	/* Set the command line */
	if ( ( rc = ieee1275_finddevice ( "/chosen", &chosen ) ) != 0 ) {
		DBGC ( &ieee1275_entry, "IEEE1275 no /chosen: %s\n",
		       strerror ( rc ) );
		return rc;
	}
	if ( ( rc = ieee1275_setprop ( chosen, "bootargs", cmdline,
				       len ) ) != 0 ) {
		DBGC ( &ieee1275_entry, "IEEE1275 could not set bootargs: "
		       "%s\n", strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Start a kernel
 *
 * @v entry		Kernel entry point
 * @v initrd		Initrd address (zero if none)
 * @v initrd_len	Initrd length
 * @v keep_end		End of the memory holding the kernel and initrd
 *
 * The memory claimed above the kept memory is released, which is the
 * last firmware call.  The caller must have loaded and flushed the
 * kernel and initrd, and shut iPXE down.  A failed release is logged
 * and ignored: it cannot be undone, and the kernel may still be able
 * to claim the memory that it needs elsewhere.
 */
void ppc64le_boot_jump ( uint64_t entry, uint64_t initrd, size_t initrd_len,
			 uint64_t keep_end ) {
	int rc;

	/* Release the memory that the kernel may want to claim */
	if ( ( rc = ieee1275_memmap_trim ( keep_end ) ) != 0 ) {
		DBGC ( &ieee1275_entry, "IEEE1275 could not release memory: "
		       "%s\n", strerror ( rc ) );
	}

	/* Enter the kernel as the firmware would */
	{
		register unsigned long r3 __asm__ ( "r3" ) = initrd;
		register unsigned long r4 __asm__ ( "r4" ) = initrd_len;
		register unsigned long r5 __asm__ ( "r5" ) = ieee1275_entry;
		register unsigned long r6 __asm__ ( "r6" ) = 0;
		register unsigned long r7 __asm__ ( "r7" ) = 0;
		register unsigned long r12 __asm__ ( "r12" ) = entry;

		__asm__ __volatile__ ( "mtctr %0\n\tbctr"
				       : : "r" ( r12 ), "r" ( r3 ), "r" ( r4 ),
					 "r" ( r5 ), "r" ( r6 ), "r" ( r7 ) );
	}
	__builtin_unreachable();
}
