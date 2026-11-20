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
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * RTAS date validation
 *
 * This is pure arithmetic on values returned by the get-time-of-day
 * RTAS call and makes no firmware call itself, so that it can be
 * unit-tested in the Linux userspace build.
 */

#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <ipxe/rtas_date.h>

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
int rtas_time_tm ( const uint32_t *rets, struct tm *tm ) {
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
