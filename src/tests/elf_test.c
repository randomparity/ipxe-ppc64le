/*
 * Copyright (C) 2026 iPXE contributors.
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

/** @file
 *
 * ELF image self-tests
 *
 * The ELF class follows the architecture, so these tests exercise
 * the 64-bit class on ppc64le and the 32-bit class elsewhere.  Both
 * fixtures describe the same layout: a segment with physical address
 * zero at a high virtual address, and a second segment with a
 * non-zero physical address.
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <ipxe/image.h>
#include <ipxe/elf.h>
#include <ipxe/test.h>

#if ELFCLASS == ELFCLASS64

/** Virtual address of first segment */
#define ELF_TEST_VADDR 0xc000000000000000ULL

/** Little-endian ELF64 image, two PT_LOAD segments, entry in the first */
static const uint8_t elf_test_image FILE_ARRAY ( BINFILE (
	0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x15, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0xc0, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x40, 0x00, 0x38, 0x00, 0x02, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00,
		0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0xc0, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xc0, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0xa1, 0xa2, 0xa3,
		0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
		0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7 ) );

/** Length of image up to the end of the first segment's data */
#define ELF_TEST_SHORT_LEN 192

#else

/** Virtual address of first segment */
#define ELF_TEST_VADDR 0xc0000000UL

/** Little-endian ELF32 image, two PT_LOAD segments, entry in the first */
static const uint8_t elf_test_image FILE_ARRAY ( BINFILE (
	0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x14, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0xc0, 0x34, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x34, 0x00, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x84, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x00, 0xc0, 0x00, 0x10, 0x00, 0x00,
		0x08, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x05, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xa0, 0xa1, 0xa2, 0xa3,
		0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
		0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7 ) );

/** Length of image up to the end of the first segment's data */
#define ELF_TEST_SHORT_LEN 132

#endif

/** Offset of entry point within the first segment */
#define ELF_TEST_ENTRY_OFFSET 8

/** Segment sizes and offsets used by the fixtures */
#define ELF_TEST_MEMSZ0 0x100
#define ELF_TEST_PADDR1 0x1000
#define ELF_TEST_MEMSZ1 0x40

/** Segments passed to the segment processor */
static struct {
	unsigned int count;
	physaddr_t dest[2];
} elf_test_log;

/**
 * Record a segment
 *
 * @v image		ELF file
 * @v phdr		ELF program header
 * @v dest		Destination address
 * @ret rc		Return status code
 */
static int elf_test_process ( struct image *image __unused,
			      const Elf_Phdr *phdr __unused,
			      physaddr_t dest ) {

	if ( elf_test_log.count < 2 )
		elf_test_log.dest[elf_test_log.count] = dest;
	elf_test_log.count++;
	return 0;
}

/**
 * Walk the segments of an image
 *
 * @v data		Image data
 * @v len		Image length
 * @v base		Load base, or NULL
 * @v entry		Entry point to fill in
 * @v max		Maximum used address to fill in
 * @ret rc		Return status code (non-zero on failure)
 */
static int elf_test_walk ( const void *data, size_t len,
			   const physaddr_t *base, physaddr_t *entry,
			   physaddr_t *max ) {
	struct image *image;
	int rc;

	memset ( &elf_test_log, 0, sizeof ( elf_test_log ) );
	image = image_memory ( "elf", data, len );
	ok ( image != NULL );
	if ( ! image )
		return -1;
	rc = elf_segments ( image, image->data, elf_test_process, base, entry,
			    max );
	unregister_image ( image );
	return rc;
}

/**
 * Perform ELF self-test
 *
 */
static void elf_test_exec ( void ) {
	uint8_t copy[sizeof ( elf_test_image )];
	Elf_Ehdr *ehdr = ( ( Elf_Ehdr * ) copy );
	physaddr_t base;
	physaddr_t entry;
	physaddr_t max;

	/* Segments are placed relative to the load base, and the
	 * entry point is translated to match
	 */
	base = 0x40000000UL;
	ok ( elf_test_walk ( elf_test_image, sizeof ( elf_test_image ), &base,
			     &entry, &max ) == 0 );
	ok ( elf_test_log.count == 2 );
	ok ( elf_test_log.dest[0] == base );
	ok ( elf_test_log.dest[1] == ( base + ELF_TEST_PADDR1 ) );
	ok ( entry == ( base + ELF_TEST_ENTRY_OFFSET ) );
	ok ( max == ( base + ELF_TEST_PADDR1 + ELF_TEST_MEMSZ1 ) );

	/* A load base of zero is valid, as is an entry point of zero */
	base = 0;
	memcpy ( copy, elf_test_image, sizeof ( copy ) );
	ehdr->e_entry = ELF_TEST_VADDR;
	ok ( elf_test_walk ( copy, sizeof ( copy ), &base, &entry,
			     &max ) == 0 );
	ok ( elf_test_log.dest[0] == 0 );
	ok ( entry == 0 );

	/* Without a load base, a segment with no physical address is
	 * placed at its virtual address
	 */
	ok ( elf_test_walk ( elf_test_image, sizeof ( elf_test_image ), NULL,
			     &entry, &max ) == 0 );
	ok ( elf_test_log.count == 2 );
	ok ( elf_test_log.dest[0] == ELF_TEST_VADDR );
	ok ( elf_test_log.dest[1] == ELF_TEST_PADDR1 );
	ok ( entry == ( ELF_TEST_VADDR + ELF_TEST_ENTRY_OFFSET ) );
	ok ( max == ( ELF_TEST_VADDR + ELF_TEST_MEMSZ0 ) );

	/* Segments that would wrap the address space are rejected */
	base = ( ~( ( physaddr_t ) 0 ) - ELF_TEST_MEMSZ0 + 1 );
	ok ( elf_test_walk ( elf_test_image, sizeof ( elf_test_image ), &base,
			     &entry, &max ) != 0 );
	ok ( elf_test_log.count == 0 );
	base = ( ~( ( physaddr_t ) 0 ) - ELF_TEST_PADDR1 + 1 );
	ok ( elf_test_walk ( elf_test_image, sizeof ( elf_test_image ), &base,
			     &entry, &max ) != 0 );

	/* Segment data outside the image is rejected */
	base = 0x40000000UL;
	ok ( elf_test_walk ( elf_test_image, ELF_TEST_SHORT_LEN, &base,
			     &entry, &max ) != 0 );

	/* An entry point outside the file-backed data is rejected */
	memcpy ( copy, elf_test_image, sizeof ( copy ) );
	ehdr->e_entry = ( ELF_TEST_VADDR + 0x20 );
	ok ( elf_test_walk ( copy, sizeof ( copy ), &base, &entry,
			     &max ) != 0 );
	ehdr->e_entry = 0x1234;
	ok ( elf_test_walk ( copy, sizeof ( copy ), &base, &entry,
			     &max ) != 0 );
}

/** ELF self-test */
struct self_test elf_test __self_test = {
	.name = "elf",
	.exec = elf_test_exec,
};
