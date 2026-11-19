#ifndef _BITS_ELF_H
#define _BITS_ELF_H

/** @file
 *
 * PowerPC64 64-bit ELF class
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <elf.h>

/** ELF executable header */
typedef Elf64_Ehdr Elf_Ehdr;

/** ELF program header */
typedef Elf64_Phdr Elf_Phdr;

/** ELF file offset */
typedef Elf64_Off Elf_Off;

/** ELF class accepted by the loader */
#define ELFCLASS ELFCLASS64

#endif /* _BITS_ELF_H */
