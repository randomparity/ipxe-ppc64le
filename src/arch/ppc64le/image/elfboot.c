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
 * ppc64le ELF kernel image
 *
 * Loads a 64-bit little-endian PowerPC ELF kernel and its initrd into
 * the memory that the memory map reports as free, and starts it
 * through the platform's boot primitive.  This file makes no firmware
 * call, so that other ppc64le platforms may share it.
 *
 * The kernel is loaded at the start of the largest free region, which
 * is clear of the iPXE image and of the external heap.  A relocatable
 * kernel copies itself to its own physical address, so it does not
 * need to be loaded there.  The initrd is placed after the kernel, and
 * is copied there from the top of the heap region once everything that
 * could still fail has been done.
 */

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/image.h>
#include <ipxe/elf.h>
#include <ipxe/init.h>
#include <ipxe/initrd.h>
#include <ipxe/uheap.h>
#include <ipxe/uaccess.h>
#include <ipxe/memmap.h>
#include <ipxe/features.h>
#include <ipxe/ppc64le_kernel.h>

FEATURE ( FEATURE_IMAGE, "ELF", DHCP_EB_FEATURE_ELF, 1 );

/**
 * Choose the kernel load base
 *
 * @v base		Load base to fill in
 * @ret rc		Return status code
 */
static int elfboot_base ( physaddr_t *base ) {
	physaddr_t start;
	physaddr_t aligned;
	size_t size;

	/* Use the start of the largest free region */
	size = memmap_largest ( &start );
	aligned = ( ( start + PPC64LE_KERNEL_ALIGN - 1 ) &
		    ~( ( physaddr_t ) PPC64LE_KERNEL_ALIGN - 1 ) );
	if ( ( ! size ) || ( aligned < start ) ||
	     ( ( aligned - start ) >= size ) )
		return -ENOSPC;
	*base = aligned;
	return 0;
}

/**
 * Place the initrd
 *
 * @v kernel_end	End of the loaded kernel
 * @v initrdsz	Initrd length to fill in
 * @v initrd		Initrd address to fill in
 * @ret rc		Return status code
 */
static int elfboot_place ( physaddr_t kernel_end, size_t *initrdsz,
			   uint64_t *initrd ) {
	struct memmap_region region;
	struct image *other;
	size_t squash_len = 0;
	int rc;

	/* Allow for every image that reshuffling will squash.  The
	 * kernel has been unregistered for execution, so is not one.
	 */
	for_each_image ( other )
		squash_len += initrd_align ( other->len );

	/* Place the initrd in the region that reshuffling leaves */
	*initrdsz = initrd_len();
	if ( ( rc = initrd_region ( *initrdsz, &region ) ) != 0 )
		return rc;
	return ppc64le_kernel_layout ( kernel_end, *initrdsz, squash_len,
				       &region, initrd );
}

/**
 * Execute ELF kernel image
 *
 * @v image		ELF kernel image
 * @ret rc		Return status code
 */
static int elfboot_exec ( struct image *image ) {
	physaddr_t base;
	physaddr_t entry;
	physaddr_t max;
	uint64_t initrd;
	physaddr_t heap;
	size_t initrdsz;
	int rc;

	/* Load the kernel into free memory */
	if ( ( rc = elfboot_base ( &base ) ) != 0 ) {
		DBGC ( image, "ELF %s found no free memory: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}
	if ( ( rc = elf_load ( image, &base, &entry, &max ) ) != 0 ) {
		DBGC ( image, "ELF %s could not load: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}
	DBGC ( image, "ELF %s loaded to [%#08lx,%#08lx), entry %#08lx\n",
	       image->name, base, max, entry );
	ppc64le_icache_flush ( base, max );

	/* Check everything that can fail before shutting down */
	if ( ( rc = elfboot_place ( max, &initrdsz, &initrd ) ) != 0 ) {
		DBGC ( image, "ELF %s cannot place initrd: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}
	if ( ( rc = ppc64le_boot_prepare ( image->cmdline ) ) != 0 )
		return rc;

	/* This is the point of no return: shutting down and
	 * reshuffling destroy the external heap.  Nothing may allocate
	 * between loading the kernel and reshuffling, or the heap
	 * could grow over it.
	 */
	heap = uheap_start;
	shutdown_boot();
	assert ( uheap_start == heap );

	/* Load the initrd */
	initrd_reshuffle();
	if ( initrdsz ) {
		DBGC ( image, "ELF %s initrd at [%#08llx,%#08llx)\n",
		       image->name, ( ( unsigned long long ) initrd ),
		       ( ( unsigned long long ) ( initrd + initrdsz ) ) );
		initrd_load_all ( phys_to_virt ( initrd ) );
	}

	/* Start the kernel, keeping the memory that it occupies */
	DBGC ( image, "ELF %s starting execution at %#08lx\n",
	       image->name, entry );
	ppc64le_boot_jump ( entry, initrd, initrdsz,
			    ( initrdsz ? ( initrd + initrdsz ) : max ) );
}

/**
 * Probe ELF kernel image
 *
 * @v image		ELF file
 * @ret rc		Return status code
 */
static int elfboot_probe ( struct image *image ) {
	return ppc64le_kernel_check ( image );
}

/** ELF image type */
struct image_type elfboot_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "ELF",
	.probe = elfboot_probe,
	.exec = elfboot_exec,
};
