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
 * RTAS reboot mechanism
 *
 * The system-reboot and power-off services reset the machine, so they
 * return only on failure.  Like the other platforms, neither shuts iPXE
 * down first.
 */

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ipxe/reboot.h>
#include <ipxe/ieee1275_rtas.h>

/** Power state meaning "no preference" for the power-off service */
#define RTAS_POWER_ANY 0xffffffffUL

/**
 * Call an RTAS service that ends the running system
 *
 * @v name		Service name
 * @v nargs		Number of argument cells
 * @v args		Argument cells
 * @ret rc		Return status code (the call returned)
 */
static int rtas_terminate ( const char *name, unsigned int nargs,
			    const uint32_t *args ) {
	uint32_t status;
	uint32_t token;
	int rc;

	if ( ( rc = ieee1275_rtas_token ( name, &token ) ) != 0 )
		return rc;
	if ( ( rc = ieee1275_rtas_call ( token, nargs, args, 1,
					 &status ) ) != 0 )
		return rc;
	DBGC ( &rtas_terminate, "RTAS %s returned status %d\n", name,
	       ( int32_t ) status );
	return ( status ? -EIO : -ECANCELED );
}

/**
 * Reboot system
 *
 * @v flags		Reboot flags
 *
 * RTAS has a single kind of reset, so @c REBOOT_WARM is ignored.
 */
static void rtas_reboot ( int flags ) {
	int rc;

	if ( flags & REBOOT_SETUP ) {
		printf ( "Cannot reboot to firmware setup; not supported\n" );
		return;
	}
	rc = rtas_terminate ( "system-reboot", 0, NULL );
	printf ( "Cannot reboot: %s\n", strerror ( rc ) );
}

/**
 * Power off system
 *
 * @ret rc		Return status code
 */
static int rtas_poweroff ( void ) {
	static const uint32_t args[2] = { RTAS_POWER_ANY, RTAS_POWER_ANY };

	return rtas_terminate ( "power-off", 2, args );
}

PROVIDE_REBOOT ( rtas, reboot, rtas_reboot );
PROVIDE_REBOOT ( rtas, poweroff, rtas_poweroff );
