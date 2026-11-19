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
 * PowerPC timebase timer
 *
 * The timebase register is read directly.  Its frequency is
 * platform-specific, and is provided by timebase_frequency().
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ipxe/timer.h>
#include <ipxe/timebase.h>

/** Timebase frequency (ticks per second) */
static uint64_t timebase_hz;

/** Number of timebase ticks per iPXE tick */
static uint64_t timebase_ticks;

/** Colour for debug messages */
#define colour &timebase_hz

/**
 * Read the timebase
 *
 * @ret time		Timebase value
 */
static inline __attribute__ (( always_inline )) uint64_t timebase_read ( void ) {
	uint64_t time;

	__asm__ __volatile__ ( "mftb %0" : "=r" ( time ) );
	return time;
}

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long timebase_currticks ( void ) {

	return ( timebase_read() / timebase_ticks );
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void timebase_udelay ( unsigned long usecs ) {
	uint64_t start;
	uint64_t threshold;

	/* Delay until sufficient time has elapsed */
	start = timebase_read();
	threshold = ( ( usecs * timebase_hz ) / 1000000 );
	while ( ( timebase_read() - start ) < threshold ) {}
}

/**
 * Probe timer
 *
 * @ret rc		Return status code
 */
static int timebase_probe ( void ) {
	int rc;

	/* Get timebase frequency */
	if ( ( rc = timebase_frequency ( &timebase_hz ) ) != 0 ) {
		DBGC ( colour, "TIMEBASE could not determine frequency: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Calculate timebase ticks per iPXE tick */
	timebase_ticks = ( timebase_hz / TICKS_PER_SEC );
	if ( ! timebase_ticks ) {
		DBGC ( colour, "TIMEBASE frequency %lld Hz is too low\n",
		       ( ( unsigned long long ) timebase_hz ) );
		return -EIO;
	}
	DBGC ( colour, "TIMEBASE at %lld Hz\n",
	       ( ( unsigned long long ) timebase_hz ) );

	return 0;
}

/** Timebase timer */
struct timer timebase_timer __timer ( TIMER_PREFERRED ) = {
	.name = "timebase",
	.probe = timebase_probe,
	.currticks = timebase_currticks,
	.udelay = timebase_udelay,
};
