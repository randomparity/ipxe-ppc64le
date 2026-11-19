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
 * Open Firmware memory map
 *
 * The memory map is built from the memory nodes of the device tree.
 * The external heap sits in the largest region that the memory map
 * reports as usable, and the heap code never asks the firmware for
 * memory.  This provider therefore claims the heap's memory up front,
 * and reports as usable only the memory that it has claimed.  All
 * claim policy lives in this file.
 *
 * Client interface cells are 32 bits wide, so memory at or above 4GiB
 * can be neither claimed nor passed to the firmware.
 */

#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/memmap.h>
#include <ipxe/ieee1275.h>

/** Start address of the iPXE image */
extern char _prefix[];

/** End address of the iPXE image */
extern char _end[];

/** Limit of memory that may be claimed */
#define IEEE1275_MEMMAP_LIMIT 0x100000000ULL

/**
 * Low memory to leave unclaimed
 *
 * The lowest part of the claimed region is left for the firmware's
 * clients, such as the placement of a kernel at physical address zero.
 */
#define IEEE1275_MEMMAP_LOW ( 128 * 1024 * 1024 )

/** Maximum number of memory ranges */
#define IEEE1275_MEMMAP_MAX 16

/** A memory range */
struct ieee1275_memrange {
	/** Start address */
	uint64_t start;
	/** Size */
	uint64_t size;
};

/** In-use memory region for iPXE */
struct used_region ieee1275_used __used_region = {
	.name = "iPXE",
};

/** Memory map has been initialised */
static int ieee1275_memmap_ready;

/** Memory ranges present in the system */
static struct ieee1275_memrange ieee1275_ram[IEEE1275_MEMMAP_MAX];

/** Number of memory ranges present in the system */
static unsigned int ieee1275_nram;

/** Start of claimed memory (empty if equal to end) */
static uint64_t ieee1275_start = IEEE1275_MEMMAP_LIMIT;

/** End of claimed memory */
static uint64_t ieee1275_end = IEEE1275_MEMMAP_LIMIT;

/**
 * Read a value from property cells
 *
 * @v cells		Property cells
 * @v count		Number of cells in value (one or two)
 * @ret value		Value
 */
static uint64_t ieee1275_value ( const uint32_t *cells, unsigned int count ) {
	uint64_t value = be32_to_cpu ( cells[0] );

	if ( count > 1 )
		value = ( ( value << 32 ) | be32_to_cpu ( cells[1] ) );
	return value;
}

/**
 * Read a cell count property
 *
 * @v phandle		Package handle
 * @v name		Property name
 * @v dfl		Default value
 * @ret count		Cell count (zero if unsupported)
 */
static unsigned int ieee1275_cells ( ieee1275_phandle_t phandle,
				     const char *name, unsigned int dfl ) {
	uint32_t cell;
	size_t size;

	if ( ieee1275_getprop ( phandle, name, &cell, sizeof ( cell ),
				&size ) != 0 )
		return dfl;
	cell = be32_to_cpu ( cell );
	return ( ( ( size == sizeof ( cell ) ) && ( cell >= 1 ) &&
		   ( cell <= 2 ) ) ? cell : 0 );
}

/**
 * Read memory ranges from a property
 *
 * @v phandle		Package handle
 * @v name		Property name
 * @v acells		Number of address cells
 * @v scells		Number of size cells
 * @v ranges		Ranges to fill in
 * @ret count		Number of ranges (zero if the property is absent)
 */
static unsigned int ieee1275_ranges ( ieee1275_phandle_t phandle,
				      const char *name, unsigned int acells,
				      unsigned int scells,
				      struct ieee1275_memrange *ranges ) {
	uint32_t cells[ IEEE1275_MEMMAP_MAX * 4 ];
	unsigned int stride = ( acells + scells );
	unsigned int count;
	unsigned int i;
	size_t size;

	if ( ieee1275_getprop ( phandle, name, cells, sizeof ( cells ),
				&size ) != 0 )
		return 0;
	if ( size > sizeof ( cells ) )
		size = sizeof ( cells );
	count = ( size / ( stride * sizeof ( cells[0] ) ) );
	if ( count > IEEE1275_MEMMAP_MAX )
		count = IEEE1275_MEMMAP_MAX;
	for ( i = 0 ; i < count ; i++ ) {
		ranges[i].start = ieee1275_value ( &cells[ i * stride ],
						   acells );
		ranges[i].size = ieee1275_value ( &cells[ ( i * stride ) +
							  acells ], scells );
	}
	return count;
}

/**
 * Check if a node is a memory node
 *
 * @v phandle		Package handle
 * @ret is_memory	Node is a memory node
 */
static int ieee1275_is_memory ( ieee1275_phandle_t phandle ) {
	char type[ sizeof ( "memory" ) ];
	size_t size;

	return ( ( ieee1275_getprop ( phandle, "device_type", type,
				      sizeof ( type ), &size ) == 0 ) &&
		 ( size == sizeof ( type ) ) &&
		 ( memcmp ( type, "memory", sizeof ( type ) ) == 0 ) );
}

/**
 * Choose and claim memory for the external heap
 *
 * @v avail		Free ranges
 * @v count		Number of free ranges
 *
 * Claims the free memory below 4GiB and above the iPXE image in the
 * largest free range, except for the low memory left for the
 * firmware's clients.
 */
static void ieee1275_claim_heap ( const struct ieee1275_memrange *avail,
				  unsigned int count ) {
	uint64_t image_end = ( ( virt_to_phys ( _end ) + PAGE_SIZE - 1 ) &
			       ~( ( uint64_t ) PAGE_SIZE - 1 ) );
	uint64_t start;
	uint64_t end;
	uint64_t low;
	uint64_t best = 0;
	unsigned long base;
	unsigned int i;
	int rc;

	/* Find the largest usable free range */
	for ( i = 0 ; i < count ; i++ ) {
		DBGC ( &ieee1275_ram, "IEEE1275MEM free [%#08llx,+%#08llx)\n",
		       ( ( unsigned long long ) avail[i].start ),
		       ( ( unsigned long long ) avail[i].size ) );
		if ( avail[i].start > IEEE1275_MEMMAP_LIMIT )
			continue;
		start = avail[i].start;
		end = ( ( avail[i].size > ( IEEE1275_MEMMAP_LIMIT - start ) ) ?
			IEEE1275_MEMMAP_LIMIT : ( start + avail[i].size ) );
		if ( start < image_end )
			start = image_end;
		if ( ( start < end ) && ( ( end - start ) > best ) ) {
			best = ( end - start );
			ieee1275_start = start;
			ieee1275_end = end;
		}
	}
	if ( ! best ) {
		DBGC ( &ieee1275_ram, "IEEE1275MEM found no free memory\n" );
		ieee1275_start = ieee1275_end = IEEE1275_MEMMAP_LIMIT;
		return;
	}

	/* Leave low memory unclaimed, and align the region to be claimed */
	low = ( ( best / 2 ) < IEEE1275_MEMMAP_LOW ) ? ( best / 2 ) :
		IEEE1275_MEMMAP_LOW;
	ieee1275_start = ( ( ieee1275_start + low + PAGE_SIZE - 1 ) &
			   ~( ( uint64_t ) PAGE_SIZE - 1 ) );
	ieee1275_end &= ~( ( uint64_t ) PAGE_SIZE - 1 );

	/* Claim memory */
	if ( ( ieee1275_start >= ieee1275_end ) ||
	     ( ( rc = ieee1275_claim ( ieee1275_start,
				       ( ieee1275_end - ieee1275_start ), 0,
				       &base ) ) != 0 ) ||
	     ( base != ieee1275_start ) ) {
		DBGC ( &ieee1275_ram, "IEEE1275MEM could not claim "
		       "[%#08llx,%#08llx)\n",
		       ( ( unsigned long long ) ieee1275_start ),
		       ( ( unsigned long long ) ieee1275_end ) );
		ieee1275_start = ieee1275_end = IEEE1275_MEMMAP_LIMIT;
		return;
	}
	DBGC ( &ieee1275_ram, "IEEE1275MEM claimed [%#08llx,%#08llx)\n",
	       ( ( unsigned long long ) ieee1275_start ),
	       ( ( unsigned long long ) ieee1275_end ) );
}

/**
 * Initialise memory map
 *
 */
static void ieee1275_memmap_init ( void ) {
	struct ieee1275_memrange avail[IEEE1275_MEMMAP_MAX];
	struct ieee1275_memrange range[IEEE1275_MEMMAP_MAX];
	ieee1275_phandle_t root;
	ieee1275_phandle_t node;
	unsigned int acells;
	unsigned int scells;
	unsigned int navail = 0;
	unsigned int count;
	unsigned int i;
	int rc;

	/* Do this once only, whether or not it succeeds */
	ieee1275_memmap_ready = 1;

	/* Record the image as in use */
	memmap_use ( &ieee1275_used, virt_to_phys ( _prefix ),
		     ( _end - _prefix ) );

	/* Find the memory nodes, which are children of the root node */
	if ( ( rc = ieee1275_finddevice ( "/", &root ) ) != 0 ) {
		DBGC ( &ieee1275_ram, "IEEE1275MEM no root node: %s\n",
		       strerror ( rc ) );
		return;
	}
	acells = ieee1275_cells ( root, "#address-cells", 2 );
	scells = ieee1275_cells ( root, "#size-cells", 1 );
	if ( ( ! acells ) || ( ! scells ) ) {
		DBGC ( &ieee1275_ram, "IEEE1275MEM unsupported cell sizes\n" );
		return;
	}
	for ( rc = ieee1275_child ( root, &node ) ; rc == 0 ;
	      rc = ieee1275_peer ( node, &node ) ) {
		if ( ! ieee1275_is_memory ( node ) )
			continue;

		/* Record memory present, and memory not in use by firmware */
		count = ieee1275_ranges ( node, "reg", acells, scells, range );
		for ( i = 0 ; ( ( i < count ) &&
				( ieee1275_nram < IEEE1275_MEMMAP_MAX ) ) ; i++ )
			ieee1275_ram[ieee1275_nram++] = range[i];
		count = ieee1275_ranges ( node, "available", acells, scells,
					  range );
		for ( i = 0 ; ( ( i < count ) &&
				( navail < IEEE1275_MEMMAP_MAX ) ) ; i++ )
			avail[navail++] = range[i];
	}

	/* Claim memory for the external heap */
	ieee1275_claim_heap ( avail, navail );
}

/**
 * Describe memory region from system memory map
 *
 * @v min		Minimum address
 * @v hide		Hide in-use regions from the memory map
 * @v region		Region descriptor to fill in
 */
static void ieee1275_memmap_describe ( uint64_t min, int hide,
				       struct memmap_region *region ) {
	unsigned int i;

	/* Initialise on first use */
	if ( ! ieee1275_memmap_ready )
		ieee1275_memmap_init();

	/* Describe memory present, and treat all but the claimed memory
	 * and all memory beyond the reach of the client interface as
	 * unusable.
	 */
	memmap_init ( min, region );
	for ( i = 0 ; i < ieee1275_nram ; i++ ) {
		memmap_update ( region, ieee1275_ram[i].start,
				ieee1275_ram[i].size, MEMMAP_FL_MEMORY,
				"memory" );
	}
	memmap_update ( region, 0, ieee1275_start, MEMMAP_FL_RESERVED,
			"unclaimed" );
	memmap_update ( region, ieee1275_end, -ieee1275_end,
			MEMMAP_FL_RESERVED, "unclaimed" );
	memmap_update ( region, IEEE1275_MEMMAP_LIMIT, -IEEE1275_MEMMAP_LIMIT,
			MEMMAP_FL_INACCESSIBLE, NULL );

	/* Hide in-use regions, if applicable */
	if ( hide )
		memmap_update_used ( region );
}

PROVIDE_MEMMAP ( ieee1275, memmap_describe, ieee1275_memmap_describe );
PROVIDE_MEMMAP_INLINE ( ieee1275, memmap_sync );
