#ifndef _IPXE_IEEE1275_H
#define _IPXE_IEEE1275_H

/** @file
 *
 * Open Firmware client interface
 *
 * Every call into the firmware goes through ieee1275_call(), which
 * owns the byte-swapping of the argument cells and, through
 * ppc64le_be_call(), the switch to big-endian mode.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/** An Open Firmware cell (a 32-bit value in host byte order) */
typedef uint32_t ieee1275_cell_t;

/** An Open Firmware package handle */
typedef ieee1275_cell_t ieee1275_phandle_t;

/** An Open Firmware instance handle */
typedef ieee1275_cell_t ieee1275_ihandle_t;

/** Maximum number of argument and return cells in a call */
#define IEEE1275_MAX_CELLS 8

/** Client interface entry point (zero if not entered via firmware) */
extern unsigned long ieee1275_entry;

/**
 * Convert pointer to a firmware cell
 *
 * @v ptr		Pointer
 * @ret cell		Cell value
 *
 * The client interface has 32-bit cells.  The image is linked below
 * 2GiB (asserted by the linker script), as are the stack and the heap
 * within it.  The external heap is claimed below 4GiB by the memory
 * map provider.  Any pointer to them therefore fits in a cell.
 */
static inline ieee1275_cell_t ieee1275_ptr ( const void *ptr ) {
	assert ( ( ( unsigned long ) ptr >> 32 ) == 0 );
	return ( ( ieee1275_cell_t ) ( unsigned long ) ptr );
}

extern uint64_t ppc64le_be_call ( unsigned long entry, unsigned long arg0,
				  unsigned long arg1, unsigned long arg2 );
extern int ieee1275_call ( const char *service, unsigned int nargs,
			   unsigned int nrets, const ieee1275_cell_t *args,
			   ieee1275_cell_t *rets );
extern int ieee1275_finddevice ( const char *path,
				 ieee1275_phandle_t *phandle );
extern int ieee1275_child ( ieee1275_phandle_t parent,
			    ieee1275_phandle_t *child );
extern int ieee1275_peer ( ieee1275_phandle_t phandle,
			   ieee1275_phandle_t *sibling );
extern int ieee1275_package_to_path ( ieee1275_phandle_t phandle, char *buf,
				      size_t len );
extern int ieee1275_open ( const char *path, ieee1275_ihandle_t *ihandle );
extern void ieee1275_close ( ieee1275_ihandle_t ihandle );
extern int ieee1275_claim ( unsigned long virt, size_t size, size_t align,
			    unsigned long *base );
extern int ieee1275_getprop ( ieee1275_phandle_t phandle, const char *name,
			      void *buf, size_t len, size_t *size );
extern int ieee1275_read ( ieee1275_ihandle_t ihandle, void *data, size_t len );
extern int ieee1275_write ( ieee1275_ihandle_t ihandle, const void *data,
			    size_t len );
extern void ieee1275_exit ( void );

#endif /* _IPXE_IEEE1275_H */
