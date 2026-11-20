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
FILE_SECBOOT ( FORBIDDEN );

/** @file
 *
 * RTAS time source
 *
 * The time of day is trusted as the platform clock, as it is on other
 * platforms.  An RTAS error or an implausible date is never converted
 * into a time: the source then behaves like the null time source, whose
 * value precedes the start of validity of any certificate.
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ipxe/timer.h>
#include <ipxe/time.h>
#include <ipxe/ieee1275_rtas.h>

/** Number of attempts to read the time while RTAS is busy */
#define RTAS_TIME_TRIES 3

/** Delay between attempts (in milliseconds) */
#define RTAS_TIME_RETRY_MS 10

/** RTAS "busy, try again" status */
#define RTAS_STATUS_BUSY -2

/** RTAS first "extended delay" status */
#define RTAS_STATUS_DELAY_MIN 9900

/** RTAS last "extended delay" status */
#define RTAS_STATUS_DELAY_MAX 9905

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

/**
 * Check whether RTAS status means "try again"
 *
 * @v status		RTAS status
 * @ret is_busy		Status asks for a retry
 */
static int rtas_is_busy ( uint32_t status ) {

	return ( ( ( int32_t ) status == RTAS_STATUS_BUSY ) ||
		 ( ( status >= RTAS_STATUS_DELAY_MIN ) &&
		   ( status <= RTAS_STATUS_DELAY_MAX ) ) );
}

/**
 * Check whether a year is a leap year
 *
 * @v year		Year
 * @ret is_leap		Year is a leap year
 */
static int rtas_is_leap ( uint32_t year ) {

	return ( ( ( year % 4 ) == 0 ) &&
		 ( ( ( year % 100 ) != 0 ) || ( ( year % 400 ) == 0 ) ) );
}

/**
 * Convert RTAS date and time to broken-down time
 *
 * @v rets		get-time-of-day return cells
 * @v tm		Broken-down time to fill in
 * @ret rc		Return status code
 *
 * Every field is range-checked as an unsigned value before it is
 * narrowed.  Leap seconds are not reported by RTAS and are rejected.
 */
static int rtas_tm ( const uint32_t *rets, struct tm *tm ) {
	static const uint8_t days[12] =
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	uint32_t year = rets[RTAS_TIME_YEAR];
	uint32_t month = rets[RTAS_TIME_MONTH];
	uint32_t day = rets[RTAS_TIME_DAY];
	unsigned int mdays;

	if ( ( year < RTAS_YEAR_MIN ) || ( year > RTAS_YEAR_MAX ) ||
	     ( month < 1 ) || ( month > 12 ) || ( day < 1 ) ||
	     ( rets[RTAS_TIME_HOUR] > 23 ) || ( rets[RTAS_TIME_MINUTE] > 59 ) ||
	     ( rets[RTAS_TIME_SECOND] > 59 ) )
		return -EINVAL;
	mdays = days[ month - 1 ];
	if ( ( month == 2 ) && rtas_is_leap ( year ) )
		mdays++;
	if ( day > mdays )
		return -EINVAL;

	tm->tm_year = ( year - 1900 );
	tm->tm_mon = ( month - 1 );
	tm->tm_mday = day;
	tm->tm_hour = rets[RTAS_TIME_HOUR];
	tm->tm_min = rets[RTAS_TIME_MINUTE];
	tm->tm_sec = rets[RTAS_TIME_SECOND];
	return 0;
}

/**
 * Read RTAS time of day
 *
 * @v time		Time, in seconds since the Epoch
 * @ret rc		Return status code
 */
static int rtas_read_time ( time_t *time ) {
	uint32_t rets[RTAS_TIME_CELLS];
	uint32_t token;
	struct tm tm;
	unsigned int i;
	int rc;

	/* Read the time, waiting while RTAS is busy */
	if ( ( rc = ieee1275_rtas_token ( "get-time-of-day", &token ) ) != 0 )
		return rc;
	for ( i = 0 ; i < RTAS_TIME_TRIES ; i++ ) {
		if ( ( rc = ieee1275_rtas_call ( token, 0, NULL,
						 RTAS_TIME_CELLS, rets ) ) != 0 )
			return rc;
		if ( ! rtas_is_busy ( rets[RTAS_TIME_STATUS] ) )
			break;
		mdelay ( RTAS_TIME_RETRY_MS );
	}
	if ( rets[RTAS_TIME_STATUS] != 0 ) {
		DBGC ( &rtas_read_time, "RTAS get-time-of-day status %d\n",
		       ( int32_t ) rets[RTAS_TIME_STATUS] );
		return -EIO;
	}

	/* Convert to seconds since the Epoch */
	if ( ( rc = rtas_tm ( rets, &tm ) ) != 0 ) {
		DBGC ( &rtas_read_time, "RTAS implausible date %u-%u-%u "
		       "%u:%u:%u\n", rets[RTAS_TIME_YEAR],
		       rets[RTAS_TIME_MONTH], rets[RTAS_TIME_DAY],
		       rets[RTAS_TIME_HOUR], rets[RTAS_TIME_MINUTE],
		       rets[RTAS_TIME_SECOND] );
		return rc;
	}
	*time = mktime ( &tm );
	return 0;
}

/**
 * Get current time in seconds
 *
 * @ret time		Time, in seconds
 */
static time_t rtas_now ( void ) {
	time_t time;
	int rc;

	if ( ( rc = rtas_read_time ( &time ) ) != 0 ) {
		DBGC ( &rtas_read_time, "RTAS time unavailable: %s\n",
		       strerror ( rc ) );
		return ( currticks() / TICKS_PER_SEC );
	}
	return time;
}

PROVIDE_TIME ( rtas, time_now, rtas_now );
