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
 * Open Firmware timebase frequency
 *
 */

#include <errno.h>
#include <byteswap.h>
#include <ipxe/ieee1275.h>
#include <ipxe/timebase.h>

/**
 * Get timebase frequency
 *
 * @v frequency		Frequency to fill in (ticks per second)
 * @ret rc		Return status code
 *
 * The frequency is a property of each CPU node.  All CPUs share a
 * timebase, so the first CPU node is used.
 */
int timebase_frequency ( uint64_t *frequency ) {
	ieee1275_phandle_t cpus;
	ieee1275_phandle_t cpu;
	uint32_t cells[2];
	size_t size;
	int rc;

	/* Find the first CPU node */
	if ( ( rc = ieee1275_finddevice ( "/cpus", &cpus ) ) != 0 )
		return rc;
	if ( ( rc = ieee1275_child ( cpus, &cpu ) ) != 0 )
		return rc;

	/* Read the frequency, which is one or two cells */
	if ( ( rc = ieee1275_getprop ( cpu, "timebase-frequency", cells,
				       sizeof ( cells ), &size ) ) != 0 )
		return rc;
	if ( size == sizeof ( cells[0] ) ) {
		*frequency = be32_to_cpu ( cells[0] );
	} else if ( size == sizeof ( cells ) ) {
		*frequency = ( ( ( uint64_t ) be32_to_cpu ( cells[0] ) << 32 ) |
			       be32_to_cpu ( cells[1] ) );
	} else {
		return -EINVAL;
	}
	return ( *frequency ? 0 : -EINVAL );
}
