#ifndef _BITS_STDINT_H
#define _BITS_STDINT_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

typedef __SIZE_TYPE__		size_t;
typedef signed long		ssize_t;
typedef signed long		off_t;

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long	uint64_t;

typedef signed char		int8_t;
typedef signed short		int16_t;
typedef signed int		int32_t;
typedef signed long long	int64_t;

typedef unsigned long		physaddr_t;
typedef unsigned long		intptr_t;

/* The imported Xen PowerPC public header (xen/arch-ppc.h) uses these
 * types only under __XEN__ or __XEN_TOOLS__, but needs them in every
 * build that includes <xen/xen.h>.  Provide them here, as the Xen
 * RISC-V and Arm headers do for themselves.
 */
#ifndef int64_aligned_t
#define int64_aligned_t int64_t __attribute__ (( __aligned__ ( 8 ) ))
#endif
#ifndef uint64_aligned_t
#define uint64_aligned_t uint64_t __attribute__ (( __aligned__ ( 8 ) ))
#endif

#endif /* _BITS_STDINT_H */
