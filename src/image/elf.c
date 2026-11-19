/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * ELF image format
 *
 * A "pure" ELF image is not a bootable image.  There are various
 * bootable formats based upon ELF (e.g. Multiboot), which share
 * common ELF-related functionality.
 */

#include <string.h>
#include <errno.h>
#include <elf.h>
#include <ipxe/segment.h>
#include <ipxe/image.h>
#include <ipxe/uaccess.h>
#include <ipxe/elf.h>

/**
 * Load ELF segment into memory
 *
 * @v image		ELF file
 * @v phdr		ELF program header
 * @v dest		Destination address
 * @ret rc		Return status code
 */
static int elf_load_segment ( struct image *image, const Elf_Phdr *phdr,
			      physaddr_t dest ) {
	void *buffer = phys_to_virt ( dest );
	int rc;

	DBGC ( image, "ELF %s loading segment [%lx,%lx) to [%lx,%lx,%lx)\n",
	       image->name, ( ( unsigned long ) phdr->p_offset ),
	       ( ( unsigned long ) ( phdr->p_offset + phdr->p_filesz ) ),
	       ( ( unsigned long ) dest ),
	       ( ( unsigned long ) ( dest + phdr->p_filesz ) ),
	       ( ( unsigned long ) ( dest + phdr->p_memsz ) ) );

	/* Verify and prepare segment */
	if ( ( rc = prep_segment ( buffer, phdr->p_filesz,
				   phdr->p_memsz ) ) != 0 ) {
		DBGC ( image, "ELF %s could not prepare segment: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Copy image to segment */
	memcpy ( buffer, ( image->data + phdr->p_offset ), phdr->p_filesz );

	return 0;
}

/**
 * Process ELF segment
 *
 * @v image		ELF file
 * @v ehdr		ELF executable header
 * @v phdr		ELF program header
 * @v process		Segment processor
 * @v base		Load base, or NULL to use the image's own addresses
 * @v entry		Entry point (updated if found)
 * @v max		Maximum used address (updated)
 * @v found		Entry point found flag (updated)
 * @ret rc		Return status code
 */
static int elf_segment ( struct image *image, const Elf_Ehdr *ehdr,
			 const Elf_Phdr *phdr,
			 int ( * process ) ( struct image *image,
					     const Elf_Phdr *phdr,
					     physaddr_t dest ),
			 const physaddr_t *base, physaddr_t *entry,
			 physaddr_t *max, int *found ) {
	physaddr_t dest;
	physaddr_t end;
	physaddr_t e_offset;
	int rc;

	/* Do nothing for non-PT_LOAD segments */
	if ( phdr->p_type != PT_LOAD )
		return 0;

	/* Check segment lies within image */
	if ( ( image->len < phdr->p_offset ) ||
	     ( ( image->len - phdr->p_offset ) < phdr->p_filesz ) ) {
		DBGC ( image, "ELF %s segment outside image\n", image->name );
		return -ENOEXEC;
	}

	/* Find start address */
	if ( base ) {

		/* Place segment relative to the caller's load base.
		 * A physical address of zero is an offset of zero,
		 * not an absent address.  Ensure that the placed
		 * segment, and hence any entry point within its
		 * file-backed portion, cannot wrap the address space.
		 */
		if ( ( phdr->p_filesz > phdr->p_memsz ) ||
		     ( phdr->p_paddr > ( ~( ( physaddr_t ) 0 ) - *base ) ) ||
		     ( phdr->p_memsz > ( ~( ( physaddr_t ) 0 ) -
					 ( *base + phdr->p_paddr ) ) ) ) {
			DBGC ( image, "ELF %s segment exceeds address space\n",
			       image->name );
			return -ENOEXEC;
		}
		dest = ( *base + phdr->p_paddr );

	} else {

		/* Use physical address for preference, fall back to
		 * virtual address if no physical address supplied.
		 */
		dest = phdr->p_paddr;
		if ( ! dest )
			dest = phdr->p_vaddr;
		if ( ! dest ) {
			DBGC ( image, "ELF %s segment loads to physical "
			       "address 0\n", image->name );
			return -ENOEXEC;
		}
	}
	end = ( dest + phdr->p_memsz );

	/* Update maximum used address, if applicable */
	if ( end > *max )
		*max = end;

	/* Process segment */
	if ( ( rc = process ( image, phdr, dest ) ) != 0 )
		return rc;

	/* Set execution address, if it lies within this segment.  A
	 * caller-supplied base relocates the segment, so only the
	 * virtual address can identify the entry point in that case.
	 */
	if ( ( ! base ) &&
	     ( ( e_offset = ( ehdr->e_entry - dest ) ) < phdr->p_filesz ) ) {
		*entry = ehdr->e_entry;
		*found = 1;
		DBGC ( image, "ELF %s found physical entry point at %lx\n",
		       image->name, ( ( unsigned long ) *entry ) );
	} else if ( ( e_offset = ( ehdr->e_entry - phdr->p_vaddr ) )
		    < phdr->p_filesz ) {
		if ( ! *found ) {
			*entry = ( dest + e_offset );
			*found = 1;
			DBGC ( image, "ELF %s found virtual entry point at %lx"
			       " (virt %lx)\n", image->name,
			       ( ( unsigned long ) *entry ),
			       ( ( unsigned long ) ehdr->e_entry ) );
		}
	}

	return 0;
}

/**
 * Process ELF segments
 *
 * @v image		ELF file
 * @v ehdr		ELF executable header
 * @v process		Segment processor
 * @v base		Load base, or NULL to use the image's own addresses
 * @ret entry		Entry point, if found
 * @ret max		Maximum used address
 * @ret rc		Return status code
 *
 * If a load base is supplied, each segment is placed at the load
 * base plus its physical address (which may be zero), and the
 * returned entry point is translated to the corresponding location
 * within the placed segment.  If no load base is supplied, each
 * segment is placed at its own physical address (or at its virtual
 * address, if there is no physical address).
 */
int elf_segments ( struct image *image, const Elf_Ehdr *ehdr,
		   int ( * process ) ( struct image *image,
				       const Elf_Phdr *phdr,
				       physaddr_t dest ),
		   const physaddr_t *base, physaddr_t *entry,
		   physaddr_t *max ) {
	const Elf_Phdr *phdr;
	Elf_Off phoff;
	unsigned int phnum;
	int found = 0;
	int rc;

	/* Initialise maximum used address */
	*max = 0;

	/* Invalidate entry point */
	*entry = 0;

	/* Read and process ELF program headers */
	for ( phoff = ehdr->e_phoff , phnum = ehdr->e_phnum ; phnum ;
	      phoff += ehdr->e_phentsize, phnum-- ) {
		if ( ( image->len < phoff ) ||
		     ( ( image->len - phoff ) < sizeof ( *phdr ) ) ) {
			DBGC ( image, "ELF %s program header %d outside "
			       "image\n", image->name, phnum );
			return -ENOEXEC;
		}
		phdr = ( image->data + phoff );
		if ( ( rc = elf_segment ( image, ehdr, phdr, process, base,
					  entry, max, &found ) ) != 0 )
			return rc;
	}

	/* Check for a valid execution address */
	if ( ! found ) {
		DBGC ( image, "ELF %s entry point %lx outside image\n",
		       image->name, ( ( unsigned long ) ehdr->e_entry ) );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Load ELF image into memory
 *
 * @v image		ELF file
 * @v base		Load base, or NULL to use the image's own addresses
 * @ret entry		Entry point
 * @ret max		Maximum used address
 * @ret rc		Return status code
 *
 * See elf_segments() for the meaning of the load base.
 */
int elf_load ( struct image *image, const physaddr_t *base, physaddr_t *entry,
	       physaddr_t *max ) {
	static const uint8_t e_ident[] = {
		[EI_MAG0]	= ELFMAG0,
		[EI_MAG1]	= ELFMAG1,
		[EI_MAG2]	= ELFMAG2,
		[EI_MAG3]	= ELFMAG3,
		[EI_CLASS]	= ELFCLASS,
	};
	const Elf_Ehdr *ehdr;
	int rc;

	/* Read ELF header */
	if ( image->len < sizeof ( *ehdr ) ) {
		DBGC ( image, "ELF %s too short for ELF header\n",
		       image->name );
		return -ENOEXEC;
	}
	ehdr = image->data;
	if ( memcmp ( ehdr->e_ident, e_ident, sizeof ( e_ident ) ) != 0 ) {
		DBGC ( image, "ELF %s has invalid signature\n", image->name );
		return -ENOEXEC;
	}

	/* Only little-endian 64-bit images are supported */
	if ( ( ELFCLASS == ELFCLASS64 ) &&
	     ( ehdr->e_ident[EI_DATA] != ELFDATA2LSB ) ) {
		DBGC ( image, "ELF %s is not little-endian\n", image->name );
		return -ENOEXEC;
	}

	/* Load ELF segments into memory */
	if ( ( rc = elf_segments ( image, ehdr, elf_load_segment, base,
				   entry, max ) ) != 0 )
		return rc;

	return 0;
}
