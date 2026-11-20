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

/** @file
 *
 * ppc64le kernel probe and placement self-tests
 *
 * The helpers are part of the ppc64le architecture code, so the test
 * body is empty in builds for any other architecture.
 */

#ifdef __powerpc64__

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <elf.h>
#include <ipxe/image.h>
#include <ipxe/memmap.h>
#include <ipxe/initrd.h>
#include <ipxe/ppc64le_kernel.h>
#include <ipxe/test.h>

/**
 * Check an ELF header
 *
 * @v ehdr		ELF header
 * @v len		Length of image
 * @ret rc		Return status code
 */
static int ppc64le_kernel_test_check ( const Elf64_Ehdr *ehdr, size_t len ) {
	struct image *image;
	int rc;

	image = image_memory ( "kernel", ehdr, len );
	ok ( image != NULL );
	if ( ! image )
		return -1;
	rc = ppc64le_kernel_check ( image );
	unregister_image ( image );
	return rc;
}

/**
 * Check a region layout
 *
 * @v initrd_len	Length of initrd
 * @v squash_len	Length of squashed images
 * @v min		Start of region
 * @v max		Last address of region
 * @v initrd		Initrd address to fill in
 * @ret rc		Return status code
 */
static int ppc64le_kernel_test_layout ( size_t initrd_len, size_t squash_len,
					uint64_t min, uint64_t max,
					uint64_t *initrd ) {
	struct memmap_region region;

	memmap_init ( min, &region );
	region.max = max;
	region.flags = MEMMAP_FL_MEMORY;
	return ppc64le_kernel_layout ( initrd_len, squash_len, &region,
				       initrd );
}

/**
 * Perform ppc64le kernel self-test
 *
 */
static void ppc64le_kernel_test_exec ( void ) {
	static const uint8_t e_ident[] = {
		ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB,
		EV_CURRENT
	};
	Elf64_Ehdr ehdr;
	uint64_t initrd;

	/* Probe accepts a little-endian ELF64 PowerPC executable */
	memset ( &ehdr, 0, sizeof ( ehdr ) );
	memcpy ( ehdr.e_ident, e_ident, sizeof ( e_ident ) );
	ehdr.e_machine = PPC64LE_EM_PPC64;
	ehdr.e_type = PPC64LE_ET_EXEC;
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) ) == 0 );

	/* Probe rejects a short image, wrong machine, type, class,
	 * data encoding and signature
	 */
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) - 1 ) != 0 );
	ehdr.e_machine = 62;
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) ) != 0 );
	ehdr.e_machine = PPC64LE_EM_PPC64;
	ehdr.e_type = 1;
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) ) != 0 );
	ehdr.e_type = PPC64LE_ET_EXEC;
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) ) != 0 );
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	ehdr.e_ident[EI_DATA] = ( ELFDATA2LSB + 1 );
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) ) != 0 );
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_ident[EI_MAG0] = 0;
	ok ( ppc64le_kernel_test_check ( &ehdr, sizeof ( ehdr ) ) != 0 );

	/* The initrd starts at the start of the region, whether or not
	 * there is one
	 */
	ok ( ppc64le_kernel_test_layout ( 0, 0, 0x8000000, 0xfffffff,
					  &initrd ) == 0 );
	ok ( initrd == 0x8000000 );
	ok ( ppc64le_kernel_test_layout ( 0x1000, 0, 0x8000000, 0xfffffff,
					  &initrd ) == 0 );
	ok ( initrd == 0x8000000 );

	/* An initrd that exactly fits the space before the squashed
	 * images is accepted, and one byte more is not
	 */
	ok ( ppc64le_kernel_test_layout ( 0x7f00000, 0x100000, 0x8000000,
					  0xfffffff, &initrd ) == 0 );
	ok ( ppc64le_kernel_test_layout ( 0x7f00001, 0x100000, 0x8000000,
					  0xfffffff, &initrd ) != 0 );

	/* The initrd length is not the sum of the aligned image lengths:
	 * cpio headers can make it longer
	 */
	ok ( ppc64le_kernel_test_layout ( 0x7f01000, 0x100000, 0x8000000,
					  0xfffffff, &initrd ) != 0 );

	/* Squashed images larger than the region are rejected, even
	 * with no initrd
	 */
	ok ( ppc64le_kernel_test_layout ( 0, 0x8000001, 0x8000000,
					  0xfffffff, &initrd ) != 0 );
	ok ( ppc64le_kernel_test_layout ( 0x1000, ~( ( size_t ) 0 ),
					  0x8000000, 0xfffffff,
					  &initrd ) != 0 );

	/* An unaligned region start is rounded up and must leave room */
	ok ( ppc64le_kernel_test_layout ( 0x1000, 0, 0x8000001, 0x8001fff,
					  &initrd ) == 0 );
	ok ( initrd == 0x8001000 );
	ok ( ppc64le_kernel_test_layout ( 0x1000, 0, 0x8000001, 0x8000fff,
					  &initrd ) != 0 );

	/* Lengths that would wrap the address space are rejected */
	ok ( ppc64le_kernel_test_layout ( ~( ( size_t ) 0 ), 0, 0x8000000,
					  0xfffffff, &initrd ) != 0 );

	/* Regions beyond the reach of the firmware, or inverted, are
	 * rejected, and the last page below 4GiB is usable
	 */
	ok ( ppc64le_kernel_test_layout ( 0x1000, 0, 0x8000000,
					  0x100000000ULL, &initrd ) != 0 );
	ok ( ppc64le_kernel_test_layout ( 0x1000, 0, 0x9000000, 0x8000000,
					  &initrd ) != 0 );
	ok ( ppc64le_kernel_test_layout ( 0x1000, 0, 0xfffff000,
					  0xffffffffULL, &initrd ) == 0 );
	ok ( initrd == 0xfffff000 );
}

/** ppc64le kernel self-test */
struct self_test ppc64le_kernel_test __self_test = {
	.name = "ppc64le_kernel",
	.exec = ppc64le_kernel_test_exec,
};

#endif /* __powerpc64__ */
