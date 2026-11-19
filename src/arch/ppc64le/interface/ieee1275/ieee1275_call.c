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
 * Open Firmware client interface calls
 *
 */

#include <stdint.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/ieee1275.h>

/** Client interface entry point, recorded by the prefix */
unsigned long ieee1275_entry;

/** An Open Firmware client interface argument array */
struct ieee1275_args {
	/** Service name */
	uint32_t service;
	/** Number of argument cells */
	uint32_t nargs;
	/** Number of return cells */
	uint32_t nrets;
	/** Argument and return cells */
	uint32_t cells[IEEE1275_MAX_CELLS];
} __attribute__ (( packed ));

/**
 * Call Open Firmware client interface
 *
 * @v service		Service name
 * @v nargs		Number of argument cells
 * @v nrets		Number of return cells
 * @v args		Argument cells (in host byte order)
 * @v rets		Return cells (in host byte order)
 * @ret rc		Return status code
 *
 * The argument array lives on the caller's stack, so that console
 * output generated while another call is being prepared cannot
 * corrupt it.
 */
int ieee1275_call ( const char *service, unsigned int nargs,
		    unsigned int nrets, const ieee1275_cell_t *args,
		    ieee1275_cell_t *rets ) {
	struct ieee1275_args block;
	unsigned int i;
	uint64_t ret;

	/* Sanity checks */
	if ( ! ieee1275_entry )
		return -ENOTSUP;
	if ( ( nargs + nrets ) > IEEE1275_MAX_CELLS )
		return -EINVAL;
	if ( ( ( ( unsigned long ) service ) | ( ( unsigned long ) &block ) )
	     >> 32 )
		return -ERANGE;

	/* Construct big-endian argument array */
	block.service = cpu_to_be32 ( ieee1275_ptr ( service ) );
	block.nargs = cpu_to_be32 ( nargs );
	block.nrets = cpu_to_be32 ( nrets );
	for ( i = 0 ; i < nargs ; i++ )
		block.cells[i] = cpu_to_be32 ( args[i] );
	for ( ; i < ( nargs + nrets ) ; i++ )
		block.cells[i] = 0;

	/* Call firmware */
	ret = ppc64le_be_call ( ieee1275_entry, ( ( unsigned long ) &block ),
				0, 0 );
	if ( ( uint32_t ) ret )
		return -EIO;

	/* Extract return cells */
	for ( i = 0 ; i < nrets ; i++ )
		rets[i] = be32_to_cpu ( block.cells[ nargs + i ] );

	return 0;
}

/**
 * Find a device by path
 *
 * @v path		Device path
 * @v phandle		Package handle to fill in
 * @ret rc		Return status code
 */
int ieee1275_finddevice ( const char *path, ieee1275_phandle_t *phandle ) {
	ieee1275_cell_t args[1] = { ieee1275_ptr ( path ) };
	int rc;

	if ( ( rc = ieee1275_call ( "finddevice", 1, 1, args, phandle ) ) != 0 )
		return rc;
	return ( ( ( int32_t ) *phandle == -1 ) ? -ENODEV : 0 );
}

/**
 * Get first child of a package
 *
 * @v parent		Parent package handle
 * @v child		Child package handle to fill in
 * @ret rc		Return status code
 */
int ieee1275_child ( ieee1275_phandle_t parent, ieee1275_phandle_t *child ) {
	int rc;

	if ( ( rc = ieee1275_call ( "child", 1, 1, &parent, child ) ) != 0 )
		return rc;
	return ( *child ? 0 : -ENOENT );
}

/**
 * Get property value
 *
 * @v phandle		Package handle
 * @v name		Property name
 * @v buf		Value buffer
 * @v len		Length of value buffer
 * @v size		Size of property value to fill in
 * @ret rc		Return status code
 */
int ieee1275_getprop ( ieee1275_phandle_t phandle, const char *name,
		       void *buf, size_t len, size_t *size ) {
	ieee1275_cell_t args[4] = {
		phandle, ieee1275_ptr ( name ), ieee1275_ptr ( buf ), len
	};
	ieee1275_cell_t ret;
	int rc;

	if ( ( rc = ieee1275_call ( "getprop", 4, 1, args, &ret ) ) != 0 )
		return rc;
	if ( ( int32_t ) ret < 0 )
		return -ENOENT;
	*size = ret;
	return 0;
}

/**
 * Read from an instance
 *
 * @v ihandle		Instance handle
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret len		Length read (zero if nothing available), or negative error
 */
int ieee1275_read ( ieee1275_ihandle_t ihandle, void *data, size_t len ) {
	ieee1275_cell_t args[3] = { ihandle, ieee1275_ptr ( data ), len };
	ieee1275_cell_t ret;
	int rc;

	if ( ( rc = ieee1275_call ( "read", 3, 1, args, &ret ) ) != 0 )
		return rc;
	if ( ( int32_t ) ret == -2 )
		return 0;
	return ( ( ( int32_t ) ret < 0 ) ? -EIO : ( int ) ret );
}

/**
 * Write to an instance
 *
 * @v ihandle		Instance handle
 * @v data		Data buffer
 * @v len		Length of data
 * @ret len		Length written, or negative error
 */
int ieee1275_write ( ieee1275_ihandle_t ihandle, const void *data,
		     size_t len ) {
	ieee1275_cell_t args[3] = { ihandle, ieee1275_ptr ( data ), len };
	ieee1275_cell_t ret;
	int rc;

	if ( ( rc = ieee1275_call ( "write", 3, 1, args, &ret ) ) != 0 )
		return rc;
	return ( ( ( int32_t ) ret < 0 ) ? -EIO : ( int ) ret );
}

/**
 * Return to the firmware
 *
 */
void ieee1275_exit ( void ) {

	ieee1275_call ( "exit", 0, 0, NULL, NULL );
}
