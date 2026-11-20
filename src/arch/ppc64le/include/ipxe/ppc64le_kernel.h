#ifndef _IPXE_PPC64LE_KERNEL_H
#define _IPXE_PPC64LE_KERNEL_H

/** @file
 *
 * ppc64le ELF kernel loading
 *
 * The probe and placement helpers are pure and shared by every ppc64le
 * platform.  The boot primitives are provided by the platform: they
 * claim the memory that the kernel is loaded into, set up whatever the
 * kernel reads its command line from, and pass control to it in the form
 * that the platform's firmware expects.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( FORBIDDEN );

#include <stdint.h>
#include <ipxe/image.h>
#include <ipxe/memmap.h>

/** ELF executable file type */
#define PPC64LE_ET_EXEC 2

/** ELF machine type for 64-bit PowerPC */
#define PPC64LE_EM_PPC64 21

/** Limit of memory that a kernel and initrd may occupy (firmware cells) */
#define PPC64LE_KERNEL_LIMIT 0x100000000ULL

extern int ppc64le_kernel_check ( struct image *image );
extern int ppc64le_kernel_layout ( size_t initrd_len, size_t squash_len,
				   const struct memmap_region *region,
				   uint64_t *initrd );
extern void ppc64le_icache_flush ( uint64_t start, uint64_t end );

/* Platform boot primitives */
extern int ppc64le_boot_claim ( size_t size, uint64_t *base );
extern void ppc64le_boot_unclaim ( void );
extern int ppc64le_boot_prepare ( const char *cmdline );
extern void __attribute__ (( noreturn ))
ppc64le_boot_jump ( uint64_t entry, uint64_t initrd, size_t initrd_len,
		    uint64_t keep_end );

#endif /* _IPXE_PPC64LE_KERNEL_H */
