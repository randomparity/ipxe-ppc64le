#ifndef _BITS_ELF_H
#define _BITS_ELF_H

/** @file
 *
 * Default ELF class (32-bit)
 *
 * This file is included only if the architecture does not provide its
 * own version of this file.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <elf.h>

/** ELF executable header */
typedef Elf32_Ehdr Elf_Ehdr;

/** ELF program header */
typedef Elf32_Phdr Elf_Phdr;

/** ELF file offset */
typedef Elf32_Off Elf_Off;

/** ELF class accepted by the loader */
#define ELFCLASS ELFCLASS32

#endif /* _BITS_ELF_H */
