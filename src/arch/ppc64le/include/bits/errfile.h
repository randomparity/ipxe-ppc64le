#ifndef _BITS_ERRFILE_H
#define _BITS_ERRFILE_H

/** @file
 *
 * PowerPC64 error file identifiers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/**
 * @addtogroup errfile Error file identifiers
 * @{
 */

#define ERRFILE_timebase	( ERRFILE_ARCH | ERRFILE_CORE | 0x00000000 )
#define ERRFILE_darn		( ERRFILE_ARCH | ERRFILE_CORE | 0x00010000 )

#define ERRFILE_ieee1275_call	( ERRFILE_ARCH | ERRFILE_OTHER | 0x00000000 )
#define ERRFILE_ieee1275_console ( ERRFILE_ARCH | ERRFILE_OTHER | 0x00010000 )
#define ERRFILE_ieee1275_timebase ( ERRFILE_ARCH | ERRFILE_OTHER | 0x00020000 )
#define ERRFILE_ieee1275_memmap ( ERRFILE_ARCH | ERRFILE_OTHER | 0x00030000 )
#define ERRFILE_ieee1275_net	( ERRFILE_ARCH | ERRFILE_OTHER | 0x00040000 )
#define ERRFILE_ieee1275_rtas	( ERRFILE_ARCH | ERRFILE_OTHER | 0x00050000 )
#define ERRFILE_ieee1275_rtas_time ( ERRFILE_ARCH | ERRFILE_OTHER | 0x00060000 )
#define ERRFILE_ieee1275_rtas_reboot ( ERRFILE_ARCH | ERRFILE_OTHER | 0x00070000 )

/** @} */

#endif /* _BITS_ERRFILE_H */
