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
#include <string.h>
#include <limits.h>
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
 * Get next sibling of a package
 *
 * @v phandle		Package handle
 * @v sibling		Sibling package handle to fill in
 * @ret rc		Return status code
 */
int ieee1275_peer ( ieee1275_phandle_t phandle,
		    ieee1275_phandle_t *sibling ) {
	int rc;

	if ( ( rc = ieee1275_call ( "peer", 1, 1, &phandle, sibling ) ) != 0 )
		return rc;
	return ( *sibling ? 0 : -ENOENT );
}

/**
 * Get path of a package
 *
 * @v phandle		Package handle
 * @v buf		Path buffer
 * @v len		Length of path buffer
 * @ret rc		Return status code
 *
 * On success the path is NUL-terminated within the buffer.
 */
int ieee1275_package_to_path ( ieee1275_phandle_t phandle, char *buf,
			       size_t len ) {
	ieee1275_cell_t args[3];
	ieee1275_cell_t ret;
	int rc;

	/* Leave room for the terminator, which the firmware need not write */
	if ( ! len )
		return -ENOSPC;
	memset ( buf, 0, len );
	args[0] = phandle;
	args[1] = ieee1275_ptr ( buf );
	args[2] = ( len - 1 );
	if ( ( rc = ieee1275_call ( "package-to-path", 3, 1, args,
				    &ret ) ) != 0 )
		return rc;
	return ( ( ( ( int32_t ) ret < 0 ) || ( ret >= len ) ) ?
		 -ENOSPC : 0 );
}

/**
 * Open a device
 *
 * @v path		Device path
 * @v ihandle		Instance handle to fill in
 * @ret rc		Return status code
 */
int ieee1275_open ( const char *path, ieee1275_ihandle_t *ihandle ) {
	ieee1275_cell_t args[1] = { ieee1275_ptr ( path ) };
	int rc;

	if ( ( rc = ieee1275_call ( "open", 1, 1, args, ihandle ) ) != 0 )
		return rc;
	return ( *ihandle ? 0 : -ENODEV );
}

/**
 * Close an instance
 *
 * @v ihandle		Instance handle
 */
void ieee1275_close ( ieee1275_ihandle_t ihandle ) {

	ieee1275_call ( "close", 1, 0, &ihandle, NULL );
}

/**
 * Claim memory
 *
 * @v virt		Address to claim (or zero to let the firmware choose)
 * @v size		Size of region
 * @v align		Alignment (or zero to claim exactly at @c virt)
 * @v base		Base address of claimed region to fill in
 * @ret rc		Return status code
 *
 * All values must fit in a cell: the client interface cannot
 * describe memory at or above 4GiB.
 */
int ieee1275_claim ( unsigned long virt, size_t size, size_t align,
		     unsigned long *base ) {
	ieee1275_cell_t args[3] = { virt, size, align };
	ieee1275_cell_t ret;
	int rc;

	if ( ( ( virt | size | align ) >> 32 ) ||
	     ( size > ( 0x100000000UL - virt ) ) )
		return -ERANGE;
	if ( ( rc = ieee1275_call ( "claim", 3, 1, args, &ret ) ) != 0 )
		return rc;
	if ( ( int32_t ) ret == -1 )
		return -ENOMEM;
	*base = ret;
	return 0;
}

/**
 * Call a method of an instance
 *
 * @v method		Method name
 * @v ihandle		Instance handle
 * @v nargs		Number of method argument cells
 * @v args		Method argument cells
 * @v nrets		Number of method return cells
 * @v rets		Method return cells
 * @ret rc		Return status code
 *
 * The firmware's catch result is reported as an error, and the
 * return cells are filled in only on success.
 */
int ieee1275_call_method ( const char *method, ieee1275_ihandle_t ihandle,
			   unsigned int nargs, const ieee1275_cell_t *args,
			   unsigned int nrets, ieee1275_cell_t *rets ) {
	ieee1275_cell_t cargs[IEEE1275_MAX_CELLS];
	ieee1275_cell_t crets[IEEE1275_MAX_CELLS];
	unsigned int i;
	int rc;

	/* Construct argument cells (method name and instance first) */
	if ( ( nargs + 2 > IEEE1275_MAX_CELLS ) ||
	     ( nrets + 1 > IEEE1275_MAX_CELLS ) )
		return -EINVAL;
	cargs[0] = ieee1275_ptr ( method );
	cargs[1] = ihandle;
	for ( i = 0 ; i < nargs ; i++ )
		cargs[ i + 2 ] = args[i];

	/* Call method */
	if ( ( rc = ieee1275_call ( "call-method", ( nargs + 2 ),
				    ( nrets + 1 ), cargs, crets ) ) != 0 )
		return rc;
	if ( crets[0] )
		return -EIO;
	for ( i = 0 ; i < nrets ; i++ )
		rets[i] = crets[ i + 1 ];

	return 0;
}

/**
 * Release claimed memory
 *
 * @v virt		Address of claimed region
 * @v size		Size of region
 * @ret rc		Return status code
 */
int ieee1275_release ( unsigned long virt, size_t size ) {
	ieee1275_cell_t args[2] = { virt, size };

	if ( ( virt | size ) >> 32 )
		return -ERANGE;
	return ieee1275_call ( "release", 2, 0, args, NULL );
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
 *
 * The returned length never exceeds @c len, so that callers may use
 * it to extend a buffer of that size without further checks.
 */
int ieee1275_read ( ieee1275_ihandle_t ihandle, void *data, size_t len ) {
	ieee1275_cell_t args[3] = { ihandle, ieee1275_ptr ( data ), len };
	ieee1275_cell_t ret;
	int rc;

	if ( ( len >> 32 ) || ( len > INT_MAX ) )
		return -ERANGE;
	if ( ( rc = ieee1275_call ( "read", 3, 1, args, &ret ) ) != 0 )
		return rc;
	if ( ( int32_t ) ret == -2 )
		return 0;
	if ( ( ( int32_t ) ret < 0 ) || ( ret > len ) )
		return -EIO;
	return ret;
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
