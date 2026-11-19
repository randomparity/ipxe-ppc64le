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
 * Open Firmware console
 *
 */

#include <errno.h>
#include <byteswap.h>
#include <ipxe/ieee1275.h>
#include <ipxe/keys.h>
#include <ipxe/console.h>
#include <config/console.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_IEEE1275 ) && CONSOLE_EXPLICIT ( CONSOLE_IEEE1275 ) )
#undef CONSOLE_IEEE1275
#define CONSOLE_IEEE1275 ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_LOG )
#endif

/** Console instance handles */
static struct {
	/** Standard input */
	ieee1275_ihandle_t in;
	/** Standard output */
	ieee1275_ihandle_t out;
	/** Handles have been looked up */
	int found;
} ieee1275_console;

/** Buffered input character (if any) */
static unsigned char ieee1275_console_input;

/**
 * Look up console instance handles
 *
 * @ret rc		Return status code
 */
static int ieee1275_console_find ( void ) {
	ieee1275_phandle_t chosen;
	uint32_t cell;
	size_t size;
	int rc;

	/* Look up handles only once, whether or not this succeeds */
	if ( ieee1275_console.found )
		return ( ieee1275_console.out ? 0 : -ENODEV );
	ieee1275_console.found = 1;

	/* Find the standard output and input instances */
	if ( ( rc = ieee1275_finddevice ( "/chosen", &chosen ) ) != 0 )
		return rc;
	if ( ( rc = ieee1275_getprop ( chosen, "stdout", &cell, sizeof ( cell ),
				       &size ) ) != 0 )
		return rc;
	ieee1275_console.out = be32_to_cpu ( cell );
	if ( ieee1275_getprop ( chosen, "stdin", &cell, sizeof ( cell ),
				&size ) == 0 )
		ieee1275_console.in = be32_to_cpu ( cell );
	return 0;
}

/**
 * Print a character to the console
 *
 * @v character		Character to be printed
 */
static void ieee1275_putchar ( int character ) {
	char byte = character;

	if ( ieee1275_console_find() == 0 )
		ieee1275_write ( ieee1275_console.out, &byte, sizeof ( byte ) );
}

/**
 * Get character from the console
 *
 * @ret character	Character read from console, if any
 */
static int ieee1275_getchar ( void ) {
	int character;

	/* Consume and return buffered character, if any */
	character = ieee1275_console_input;
	ieee1275_console_input = 0;

	/* Convert DEL to backspace */
	if ( character == DEL )
		character = BACKSPACE;

	return character;
}

/**
 * Check for character ready to read from the console
 *
 * @ret True		Character available to read
 * @ret False		No character available to read
 */
static int ieee1275_iskey ( void ) {

	/* Do nothing if we already have a buffered character */
	if ( ieee1275_console_input )
		return ieee1275_console_input;

	/* Read and buffer byte from console, if any */
	if ( ( ieee1275_console_find() == 0 ) && ieee1275_console.in )
		ieee1275_read ( ieee1275_console.in, &ieee1275_console_input,
				sizeof ( ieee1275_console_input ) );

	return ieee1275_console_input;
}

/** Open Firmware console */
struct console_driver ieee1275_console_driver __console_driver = {
	.putchar = ieee1275_putchar,
	.getchar = ieee1275_getchar,
	.iskey = ieee1275_iskey,
	.usage = CONSOLE_IEEE1275,
};
