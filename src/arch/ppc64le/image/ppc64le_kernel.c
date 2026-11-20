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
 * ppc64le ELF kernel probe and placement
 *
 * This file makes no firmware call, so that it can be unit-tested in
 * the ppc64le Linux userspace build (natively or under qemu-user).  A
 * kernel is started with the privileges of the loader, so the probe and
 * the placement arithmetic are the only checks standing between a
 * downloaded image and memory that belongs to something else.
 */

#include <string.h>
#include <errno.h>
#include <elf.h>
#include <ipxe/image.h>
#include <ipxe/initrd.h>
#include <ipxe/ppc64le_kernel.h>

/**
 * Check that an image is a ppc64le kernel
 *
 * @v image		Image
 * @ret rc		Return status code
 *
 * Only little-endian 64-bit PowerPC executables are accepted.  The
 * program headers are checked by the ELF loader when the image is
 * loaded.
 */
int ppc64le_kernel_check ( struct image *image ) {
	static const uint8_t e_ident[] = {
		[EI_MAG0]	= ELFMAG0,
		[EI_MAG1]	= ELFMAG1,
		[EI_MAG2]	= ELFMAG2,
		[EI_MAG3]	= ELFMAG3,
		[EI_CLASS]	= ELFCLASS64,
		[EI_DATA]	= ELFDATA2LSB,
		[EI_VERSION]	= EV_CURRENT,
	};
	const Elf64_Ehdr *ehdr;

	/* Read ELF header */
	if ( image->len < sizeof ( *ehdr ) ) {
		DBGC ( image, "PPC64LE %s too short for ELF header\n",
		       image->name );
		return -ENOEXEC;
	}
	ehdr = image->data;
	if ( memcmp ( ehdr->e_ident, e_ident, sizeof ( e_ident ) ) != 0 ) {
		DBGC ( image, "PPC64LE %s is not a little-endian ELF64\n",
		       image->name );
		return -ENOEXEC;
	}

	/* Check machine and file type */
	if ( ( ehdr->e_machine != PPC64LE_EM_PPC64 ) ||
	     ( ehdr->e_type != PPC64LE_ET_EXEC ) ) {
		DBGC ( image, "PPC64LE %s has machine %d type %d\n",
		       image->name, ehdr->e_machine, ehdr->e_type );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Place the initrd
 *
 * @v initrd_len	Length of the initrd, including any cpio headers
 * @v squash_len	Total aligned length of the images that will be
 *			squashed to the top of the region
 * @v region		Region that the initrd may occupy after reshuffling
 * @v initrd		Initrd start address to fill in
 * @ret rc		Return status code
 *
 * The kernel is loaded into memory of its own, so the initrd starts
 * at the start of the region.  The images are squashed against the end
 * of the region before the initrd is copied, so the initrd must end
 * below their lowest address: this keeps every destination clear of
 * every source that has not yet been copied.  The whole region must
 * lie below the limit of the firmware cells.
 */
int ppc64le_kernel_layout ( size_t initrd_len, size_t squash_len,
			    const struct memmap_region *region,
			    uint64_t *initrd ) {
	uint64_t limit;
	uint64_t start;

	/* Check the region */
	if ( ( region->max >= PPC64LE_KERNEL_LIMIT ) ||
	     ( region->min > region->max ) )
		return -ERANGE;
	limit = ( region->max + 1 );

	/* The squashed images occupy the end of the region */
	if ( squash_len > ( limit - region->min ) )
		return -ENOSPC;
	limit -= squash_len;

	/* Place the initrd at the start of the region */
	start = initrd_align ( region->min );
	if ( ( start < region->min ) || ( start > limit ) ||
	     ( initrd_len > ( limit - start ) ) )
		return -ENOSPC;
	*initrd = start;

	return 0;
}

/**
 * Make loaded code visible to instruction fetch
 *
 * @v start		Start address
 * @v end		End address
 *
 * The kernel is copied with data stores.  Its first instruction is
 * fetched with the caches in whatever state the firmware left them, so
 * every line of the copy is written back and the instruction cache
 * invalidated.  The step is smaller than any cache line, which only
 * repeats work.
 */
void ppc64le_icache_flush ( uint64_t start, uint64_t end ) {
	uint64_t addr;

	for ( addr = ( start & ~31ULL ) ; addr < end ; addr += 32 ) {
		__asm__ __volatile__ ( "dcbst 0, %0" : : "r" ( addr )
				       : "memory" );
	}
	__asm__ __volatile__ ( "sync" : : : "memory" );
	for ( addr = ( start & ~31ULL ) ; addr < end ; addr += 32 ) {
		__asm__ __volatile__ ( "icbi 0, %0" : : "r" ( addr )
				       : "memory" );
	}
	__asm__ __volatile__ ( "sync\n\tisync" : : : "memory" );
}
