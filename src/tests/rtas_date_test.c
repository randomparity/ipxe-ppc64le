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

/** @file
 *
 * RTAS date validation self-tests
 *
 * The helper is part of the ppc64le architecture code, so the test
 * body is empty in builds for any other architecture.
 */

#ifdef __powerpc64__

/* Forcibly enable assertions */
#undef NDEBUG

#include <time.h>
#include <ipxe/rtas_date.h>
#include <ipxe/test.h>

/** Sentinel for checking that a rejected date leaves the output alone */
#define TM_UNTOUCHED 0x5a5a

/**
 * Check a date against rtas_time_tm()
 *
 * @v file		Test code file
 * @v line		Test code line
 * @v y			Year
 * @v mo		Month
 * @v d			Day
 * @v h			Hour
 * @v mi		Minute
 * @v s			Second
 * @v valid		Date is expected to be accepted
 */
static void rtas_date_okx ( const char *file, unsigned int line,
			    uint32_t y, uint32_t mo, uint32_t d, uint32_t h,
			    uint32_t mi, uint32_t s, int valid ) {
	uint32_t rets[RTAS_TIME_CELLS] = {
		[RTAS_TIME_YEAR] = y, [RTAS_TIME_MONTH] = mo,
		[RTAS_TIME_DAY] = d, [RTAS_TIME_HOUR] = h,
		[RTAS_TIME_MINUTE] = mi, [RTAS_TIME_SECOND] = s,
	};
	struct tm tm = { .tm_year = TM_UNTOUCHED };
	int rc;

	rc = rtas_time_tm ( rets, &tm );
	okx ( ( rc == 0 ) == valid, file, line );
	if ( valid ) {
		okx ( tm.tm_year == ( ( int ) y - 1900 ), file, line );
		okx ( tm.tm_mon == ( ( int ) mo - 1 ), file, line );
		okx ( tm.tm_mday == ( int ) d, file, line );
		okx ( tm.tm_hour == ( int ) h, file, line );
		okx ( tm.tm_min == ( int ) mi, file, line );
		okx ( tm.tm_sec == ( int ) s, file, line );
	} else {
		okx ( tm.tm_year == TM_UNTOUCHED, file, line );
	}
}
#define accepted( y, mo, d, h, mi, s )					\
	rtas_date_okx ( __FILE__, __LINE__, y, mo, d, h, mi, s, 1 )
#define rejected( y, mo, d, h, mi, s )					\
	rtas_date_okx ( __FILE__, __LINE__, y, mo, d, h, mi, s, 0 )

/**
 * Check a date converted through mktime()
 *
 * @v file		Test code file
 * @v line		Test code line
 * @v y			Year
 * @v mo		Month
 * @v d			Day
 * @v h			Hour
 * @v mi		Minute
 * @v s			Second
 * @v time		Expected seconds since the Epoch
 */
static void rtas_date_timex ( const char *file, unsigned int line,
			      uint32_t y, uint32_t mo, uint32_t d,
			      uint32_t h, uint32_t mi, uint32_t s,
			      time_t time ) {
	uint32_t rets[RTAS_TIME_CELLS] = {
		[RTAS_TIME_YEAR] = y, [RTAS_TIME_MONTH] = mo,
		[RTAS_TIME_DAY] = d, [RTAS_TIME_HOUR] = h,
		[RTAS_TIME_MINUTE] = mi, [RTAS_TIME_SECOND] = s,
	};
	struct tm tm;

	okx ( rtas_time_tm ( rets, &tm ) == 0, file, line );
	okx ( mktime ( &tm ) == time, file, line );
}
#define epoch( y, mo, d, h, mi, s, time )				\
	rtas_date_timex ( __FILE__, __LINE__, y, mo, d, h, mi, s, time )

/**
 * Perform RTAS date validation self-tests
 *
 */
static void rtas_date_test_exec ( void ) {

	/* 29 February: leap years, including the 400-year exception */
	accepted ( 2000, 2, 29, 0, 0, 0 );
	accepted ( 2024, 2, 29, 0, 0, 0 );
	accepted ( 2096, 2, 29, 23, 59, 59 );
	rejected ( 2001, 2, 29, 0, 0, 0 );
	rejected ( 2100, 2, 29, 0, 0, 0 );
	rejected ( 2200, 2, 29, 0, 0, 0 );
	accepted ( 2100, 2, 28, 0, 0, 0 );
	rejected ( 2024, 2, 30, 0, 0, 0 );

	/* Day 31 in each 30-day month, and in each 31-day month */
	rejected ( 2025, 4, 31, 0, 0, 0 );
	rejected ( 2025, 6, 31, 0, 0, 0 );
	rejected ( 2025, 9, 31, 0, 0, 0 );
	rejected ( 2025, 11, 31, 0, 0, 0 );
	accepted ( 2025, 4, 30, 0, 0, 0 );
	accepted ( 2025, 1, 31, 0, 0, 0 );
	accepted ( 2025, 12, 31, 0, 0, 0 );
	rejected ( 2025, 1, 32, 0, 0, 0 );

	/* Day and month out of range */
	rejected ( 2025, 5, 0, 0, 0, 0 );
	rejected ( 2025, 0, 1, 0, 0, 0 );
	rejected ( 2025, 13, 1, 0, 0, 0 );

	/* Time of day out of range, including leap seconds */
	rejected ( 2025, 5, 1, 24, 0, 0 );
	rejected ( 2025, 5, 1, 0, 60, 0 );
	rejected ( 2025, 5, 1, 0, 0, 60 );
	accepted ( 2025, 5, 1, 23, 59, 59 );

	/* Both ends of the accepted year range */
	rejected ( 1999, 12, 31, 23, 59, 59 );
	accepted ( 2000, 1, 1, 0, 0, 0 );
	accepted ( 2200, 12, 31, 23, 59, 59 );
	rejected ( 2201, 1, 1, 0, 0, 0 );

	/* Values are unsigned: a set top bit is out of range, not negative */
	rejected ( 0x80000000U, 5, 1, 0, 0, 0 );
	rejected ( 0xffffffffU, 5, 1, 0, 0, 0 );
	rejected ( 2025, 0x80000000U, 1, 0, 0, 0 );
	rejected ( 2025, 0xffffffffU, 1, 0, 0, 0 );
	rejected ( 2025, 5, 0x80000000U, 0, 0, 0 );
	rejected ( 2025, 5, 0xffffffffU, 0, 0, 0 );
	rejected ( 2025, 5, 1, 0x80000000U, 0, 0 );
	rejected ( 2025, 5, 1, 0, 0x80000000U, 0 );
	rejected ( 2025, 5, 1, 0, 0, 0x80000000U );
	rejected ( 2025, 5, 1, 0, 0, 0xffffffffU );

	/* Accepted dates convert to the expected seconds since the Epoch */
	epoch ( 2031, 5, 17, 8, 9, 10, 1936771750ULL );
	epoch ( 2024, 2, 29, 12, 34, 56, 1709210096ULL );
	epoch ( 2000, 2, 29, 0, 0, 0, 951782400ULL );
	epoch ( 2200, 12, 31, 23, 59, 59, 7289654399ULL );
}

/** RTAS date validation self-test */
struct self_test rtas_date_test __self_test = {
	.name = "rtas_date",
	.exec = rtas_date_test_exec,
};

#endif /* __powerpc64__ */
