#ifndef _IPXE_RTAS_DATE_H
#define _IPXE_RTAS_DATE_H

/** @file
 *
 * RTAS date validation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <time.h>

/** Earliest year accepted: an earlier reading means an unset clock */
#define RTAS_YEAR_MIN 2000

/** Latest year accepted */
#define RTAS_YEAR_MAX 2200

/** Return cells of get-time-of-day */
enum rtas_time_cell {
	RTAS_TIME_STATUS = 0,
	RTAS_TIME_YEAR,
	RTAS_TIME_MONTH,
	RTAS_TIME_DAY,
	RTAS_TIME_HOUR,
	RTAS_TIME_MINUTE,
	RTAS_TIME_SECOND,
	RTAS_TIME_NANOSECOND,
	RTAS_TIME_CELLS
};

extern int rtas_time_tm ( const uint32_t *rets, struct tm *tm );

#endif /* _IPXE_RTAS_DATE_H */
