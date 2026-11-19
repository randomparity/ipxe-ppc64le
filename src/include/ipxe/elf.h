#ifndef _IPXE_ELF_H
#define _IPXE_ELF_H

/**
 * @file
 *
 * ELF image format
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/image.h>
#include <elf.h>
#include <bits/elf.h>

extern int elf_segments ( struct image *image, const Elf_Ehdr *ehdr,
			  int ( * process ) ( struct image *image,
					      const Elf_Phdr *phdr,
					      physaddr_t dest ),
			  const physaddr_t *base, physaddr_t *entry,
			  physaddr_t *max );
extern int elf_load ( struct image *image, const physaddr_t *base,
		      physaddr_t *entry, physaddr_t *max );

#endif /* _IPXE_ELF_H */
