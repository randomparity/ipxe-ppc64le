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
 * Run-Time Abstraction Services (RTAS)
 *
 * RTAS is firmware code that runs inside the client.  It is
 * instantiated into claimed memory on first use, so that a missing or
 * broken /rtas node affects only the services that need it and never
 * startup.  The instance and the claim are released at shutdown, for
 * both an exit to the firmware and an OS boot, so that the next client
 * can instantiate its own copy.
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/init.h>
#include <ipxe/ieee1275.h>
#include <ipxe/ieee1275_rtas.h>

/** Maximum number of argument and return cells in an RTAS call */
#define RTAS_MAX_CELLS 16

/** Largest RTAS image that will be instantiated */
#define RTAS_MAX_SIZE 0x100000

/** Alignment and granularity of the RTAS claim */
#define RTAS_ALIGN 4096

/** An RTAS argument block (big-endian, and below 4GiB) */
struct rtas_args {
	/** Service token */
	uint32_t token;
	/** Number of argument cells */
	uint32_t nargs;
	/** Number of return cells (the first is the status) */
	uint32_t nrets;
	/** Argument and return cells */
	uint32_t cells[RTAS_MAX_CELLS];
} __attribute__ (( packed ));

/** An instantiated RTAS */
struct rtas {
	/** Package handle of the /rtas node */
	ieee1275_phandle_t phandle;
	/** Instance handle of the /rtas node */
	ieee1275_ihandle_t ihandle;
	/** Claimed base address (and length) */
	unsigned long base;
	/** Length of claimed region */
	size_t len;
	/** Entry point (zero if not instantiated) */
	unsigned long entry;
};

/** The instantiated RTAS */
static struct rtas rtas;

/** Permanent failure to find a usable RTAS (zero if none) */
static int rtas_absent;

/** System has been shut down: RTAS must not be instantiated again */
static int rtas_shut_down;

/**
 * Read a 32-bit property of the /rtas node
 *
 * @v phandle		Package handle
 * @v name		Property name
 * @v value		Value to fill in (in host byte order)
 * @ret rc		Return status code
 */
static int rtas_getprop ( ieee1275_phandle_t phandle, const char *name,
			  uint32_t *value ) {
	uint32_t prop;
	size_t size;
	int rc;

	if ( ( rc = ieee1275_getprop ( phandle, name, &prop, sizeof ( prop ),
				       &size ) ) != 0 )
		return rc;
	if ( size != sizeof ( prop ) )
		return -EINVAL;
	*value = be32_to_cpu ( prop );
	return 0;
}

/**
 * Release RTAS
 *
 */
static void rtas_release ( void ) {
	int rc;

	if ( rtas.ihandle )
		ieee1275_close ( rtas.ihandle );
	if ( rtas.len ) {
		rc = ieee1275_release ( rtas.base, rtas.len );
		DBGC ( &rtas, "RTAS released %#lx+%#zx: %s\n", rtas.base,
		       rtas.len, strerror ( rc ) );
	}
	memset ( &rtas, 0, sizeof ( rtas ) );
}

/**
 * Instantiate RTAS
 *
 * @ret rc		Return status code
 */
static int rtas_instantiate ( void ) {
	ieee1275_cell_t base;
	ieee1275_cell_t entry;
	uint32_t size;
	int rc;

	/* Do nothing if already instantiated, or if not possible */
	if ( rtas.entry )
		return 0;
	if ( rtas_shut_down )
		return -ENODEV;
	if ( rtas_absent )
		return rtas_absent;

	/* Locate the node and determine the size of the image */
	if ( ( rc = ieee1275_finddevice ( "/rtas", &rtas.phandle ) ) != 0 ) {
		rtas_absent = rc;
		goto err_find;
	}
	if ( ( rc = rtas_getprop ( rtas.phandle, "rtas-size", &size ) ) != 0 )
		goto err_size;
	if ( ( size == 0 ) || ( size > RTAS_MAX_SIZE ) ) {
		rc = rtas_absent = -EINVAL;
		goto err_size;
	}
	rtas.len = ( ( size + RTAS_ALIGN - 1 ) & ~( RTAS_ALIGN - 1 ) );

	/* Claim memory (below 4GiB, as the cells cannot describe more) */
	if ( ( rc = ieee1275_claim ( 0, rtas.len, RTAS_ALIGN,
				     &rtas.base ) ) != 0 ) {
		rtas.len = 0;
		goto err_claim;
	}

	/* Instantiate RTAS in the claimed memory */
	if ( ( rc = ieee1275_open ( "/rtas", &rtas.ihandle ) ) != 0 ) {
		rtas.ihandle = 0;
		goto err_open;
	}
	base = rtas.base;
	if ( ( rc = ieee1275_call_method ( "instantiate-rtas", rtas.ihandle,
					   1, &base, 1, &entry ) ) != 0 )
		goto err_instantiate;
	if ( ( entry < rtas.base ) || ( entry >= ( rtas.base + rtas.len ) ) ) {
		rc = -EINVAL;
		goto err_entry;
	}
	rtas.entry = entry;
	DBGC ( &rtas, "RTAS instantiated at %#lx+%#zx, entry %#lx\n",
	       rtas.base, rtas.len, rtas.entry );

	return 0;

 err_entry:
 err_instantiate:
 err_open:
 err_claim:
	rtas_release();
 err_size:
 err_find:
	DBGC ( &rtas, "RTAS could not be instantiated: %s\n", strerror ( rc ) );
	return rc;
}

/**
 * Get RTAS service token
 *
 * @v name		Service name
 * @v token		Token to fill in
 * @ret rc		Return status code
 */
int ieee1275_rtas_token ( const char *name, uint32_t *token ) {
	int rc;

	if ( ( rc = rtas_instantiate() ) != 0 )
		return rc;
	if ( ( ( rc = rtas_getprop ( rtas.phandle, name, token ) ) != 0 ) ||
	     ( *token == 0 ) ) {
		DBGC ( &rtas, "RTAS has no service \"%s\"\n", name );
		return -ENOTSUP;
	}
	return 0;
}

/**
 * Call RTAS
 *
 * @v token		Service token
 * @v nargs		Number of argument cells
 * @v args		Argument cells (in host byte order)
 * @v nrets		Number of return cells, including the status
 * @v rets		Return cells (in host byte order)
 * @ret rc		Return status code
 *
 * The RTAS status is the first return cell: a zero return code means
 * only that the call was made.  The argument block lives on the stack
 * (below 4GiB, like the rest of the image) and is big-endian.  The
 * value that RTAS leaves in a register is not defined, and is ignored.
 */
int ieee1275_rtas_call ( uint32_t token, unsigned int nargs,
			 const uint32_t *args, unsigned int nrets,
			 uint32_t *rets ) {
	struct rtas_args block;
	unsigned int i;
	int rc;

	/* Sanity checks */
	if ( ( nrets < 1 ) || ( ( nargs + nrets ) > RTAS_MAX_CELLS ) )
		return -EINVAL;
	if ( ( ( unsigned long ) &block ) >> 32 )
		return -ERANGE;
	if ( ( rc = rtas_instantiate() ) != 0 )
		return rc;

	/* Construct argument block */
	memset ( &block, 0, sizeof ( block ) );
	block.token = cpu_to_be32 ( token );
	block.nargs = cpu_to_be32 ( nargs );
	block.nrets = cpu_to_be32 ( nrets );
	for ( i = 0 ; i < nargs ; i++ )
		block.cells[i] = cpu_to_be32 ( args[i] );

	/* Call RTAS, which takes the argument block and its own base */
	ppc64le_be_call ( rtas.entry, ( ( unsigned long ) &block ), rtas.base,
			  0 );

	/* Extract return cells */
	for ( i = 0 ; i < nrets ; i++ )
		rets[i] = be32_to_cpu ( block.cells[ nargs + i ] );

	return 0;
}

/**
 * Start up RTAS support
 *
 */
static void rtas_startup ( void ) {

	rtas_shut_down = 0;
}

/**
 * Shut down RTAS support
 *
 * @v booting		System is about to be booted
 *
 * Shutdown functions run in the reverse order of startup, so this
 * runs after every other function, including the network devices,
 * has finished.
 */
static void rtas_shutdown ( int booting ) {

	DBGC ( &rtas, "RTAS shutdown (booting %d)\n", booting );
	rtas_shut_down = 1;
	rtas_release();
}

/** RTAS startup function */
struct startup_fn ieee1275_rtas_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "ieee1275_rtas",
	.startup = rtas_startup,
	.shutdown = rtas_shutdown,
};
