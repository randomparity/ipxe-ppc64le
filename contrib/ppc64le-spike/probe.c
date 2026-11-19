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

/** @file
 *
 * Open Firmware spike: probe body
 *
 * Runs each probe in turn and prints "SPIKE" result lines to the
 * firmware console.  Nothing here is meant to be reused as-is.
 */

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;

extern u64 be_call ( u64 entry, u64 a0, u64 a1, u64 a2 );
extern u64 entry_state[];

/** Client interface argument block (32-bit big-endian cells) */
struct ci_args {
	u32 service;
	u32 nargs;
	u32 nret;
	u32 cell[12];
};

static u32 stdout_ih;

static u32 bswap32 ( u32 v ) { return __builtin_bswap32 ( v ); }

static u64 strlen ( const char *s ) {
	u64 n = 0;
	while ( s[n] )
		n++;
	return n;
}

static void *memset ( void *p, int c, u64 n ) {
	u8 *b = p;
	while ( n-- )
		*b++ = c;
	return p;
}

/** Call a client interface service; returns ret[0] (or 0xdead if none) */
static u32 ci ( const char *service, unsigned nargs, unsigned nret,
		const u32 *in, u32 *out ) {
	static struct ci_args args;
	unsigned i;

	memset ( &args, 0, sizeof ( args ) );
	args.service = bswap32 ( ( u32 ) ( u64 ) service );
	args.nargs = bswap32 ( nargs );
	args.nret = bswap32 ( nret );
	for ( i = 0 ; i < nargs ; i++ )
		args.cell[i] = bswap32 ( in[i] );
	be_call ( entry_state[3], ( u64 ) &args, 0, 0 );
	for ( i = 0 ; i < nret ; i++ ) {
		if ( out )
			out[i] = bswap32 ( args.cell[nargs + i] );
	}
	return ( nret ? bswap32 ( args.cell[nargs] ) : 0 );
}

static u32 A ( const void *p ) { return ( u32 ) ( u64 ) p; }

static void put ( const char *s ) {
	u32 in[3] = { stdout_ih, A ( s ), strlen ( s ) };
	ci ( "write", 3, 1, in, 0 );
}

static void puthex ( u64 v ) {
	char buf[19] = "0x";
	int i;
	for ( i = 0 ; i < 16 ; i++ )
		buf[2 + i] = "0123456789abcdef"[( v >> ( 60 - 4 * i ) ) & 0xf];
	buf[18] = 0;
	put ( buf );
}

static void kv ( const char *key, u64 val ) {
	put ( "SPIKE " );
	put ( key );
	put ( " = " );
	puthex ( val );
	put ( "\r\n" );
}

static void say ( const char *key, const char *val ) {
	put ( "SPIKE " );
	put ( key );
	put ( " = " );
	put ( val );
	put ( "\r\n" );
}

static u32 finddevice ( const char *path ) {
	u32 in[1] = { A ( path ) };
	return ci ( "finddevice", 1, 1, in, 0 );
}

static int getprop ( u32 ph, const char *name, void *buf, u32 len ) {
	u32 in[4] = { ph, A ( name ), A ( buf ), len };
	return ( int ) ci ( "getprop", 4, 1, in, 0 );
}

static u32 prop32 ( u32 ph, const char *name ) {
	u32 v = 0;
	getprop ( ph, name, &v, 4 );
	return bswap32 ( v );
}

/** Print the names and lengths of all properties of a node */
static void list_props ( const char *label, u32 ph ) {
	static char prev[64], next[64];
	u32 r;

	memset ( prev, 0, sizeof ( prev ) );
	put ( "SPIKE props " );
	put ( label );
	put ( ":" );
	for ( ;; ) {
		u32 in[3] = { ph, A ( prev ), A ( next ) };
		memset ( next, 0, sizeof ( next ) );
		r = ci ( "nextprop", 3, 1, in, 0 );
		if ( r != 1 )
			break;
		put ( " " );
		put ( next );
		memset ( prev, 0, sizeof ( prev ) );
		{
			u64 i;
			for ( i = 0 ; next[i] ; i++ )
				prev[i] = next[i];
		}
	}
	put ( "\r\n" );
}

static void hexdump_prop ( const char *label, u32 ph, const char *name ) {
	static u8 buf[64];
	int len = getprop ( ph, name, buf, sizeof ( buf ) );
	int i;

	put ( "SPIKE " );
	put ( label );
	put ( " len=" );
	puthex ( ( u64 ) ( long ) len );
	put ( " data=" );
	for ( i = 0 ; i < len && i < ( int ) sizeof ( buf ) ; i++ ) {
		char c[3] = { "0123456789abcdef"[buf[i] >> 4],
			      "0123456789abcdef"[buf[i] & 15], 0 };
		put ( c );
	}
	put ( "\r\n" );
}

static u32 claim ( u32 addr, u32 size, u32 align ) {
	u32 in[3] = { addr, size, align };
	return ci ( "claim", 3, 1, in, 0 );
}

static void release ( u32 addr, u32 size ) {
	u32 in[2] = { addr, size };
	ci ( "release", 2, 0, in, 0 );
}

static void probe_entry ( void ) {
	kv ( "entry.r1 (stack)", entry_state[0] );
	kv ( "entry.r3", entry_state[1] );
	kv ( "entry.r4", entry_state[2] );
	kv ( "entry.r5 (client interface)", entry_state[3] );
	kv ( "entry.r6", entry_state[4] );
	kv ( "entry.r7", entry_state[5] );
	kv ( "entry.msr", entry_state[6] );
	kv ( "entry.msr.le (bit 0)", entry_state[6] & 1 );
	kv ( "entry.msr.sf (bit 63)", entry_state[6] >> 63 );
	kv ( "entry.msr.ee (bit 15)", ( entry_state[6] >> 15 ) & 1 );
	kv ( "entry.msr.ir (bit 5)", ( entry_state[6] >> 5 ) & 1 );
	kv ( "entry.msr.dr (bit 4)", ( entry_state[6] >> 4 ) & 1 );
	kv ( "entry.msr.hv (bit 60)", ( entry_state[6] >> 60 ) & 1 );
	kv ( "entry.entered_big_endian", entry_state[7] );
	kv ( "entry.r12 (0 if entered big-endian)", entry_state[12] );
	kv ( "entry.r13", entry_state[13] );
	kv ( "entry.r2", entry_state[10] );
	kv ( "entry.pvr", entry_state[9] );
	kv ( "entry.run_address", entry_state[8] );
	kv ( "entry.link_address (same point)", entry_state[11] );
	kv ( "ci.first_dword", *( u64 * ) entry_state[3] );
}

static void probe_tree ( void ) {
	static char text[128];
	u32 root = finddevice ( "/" );
	u32 chosen = finddevice ( "/chosen" );
	u32 mem = finddevice ( "/memory@0" );

	list_props ( "/", root );
	list_props ( "/chosen", chosen );
	list_props ( "/memory@0", mem );
	hexdump_prop ( "/memory@0 reg", mem, "reg" );
	memset ( text, 0, sizeof ( text ) );
	getprop ( chosen, "bootargs", text, sizeof ( text ) - 1 );
	say ( "/chosen bootargs", text );
	memset ( text, 0, sizeof ( text ) );
	getprop ( chosen, "bootpath", text, sizeof ( text ) - 1 );
	say ( "/chosen bootpath", text );
	hexdump_prop ( "/chosen linux,initrd-start", chosen,
		       "linux,initrd-start" );
}

static void probe_claim ( void ) {
	static const u32 where[] = {
		0x1000000, 0x10000000, 0x20000000, 0x30000000, 0x3ff00000,
		0x40000000, 0x7ff00000, 0xbff00000, 0xfff00000
	};
	u32 r;
	unsigned i;

	for ( i = 0 ; i < sizeof ( where ) / sizeof ( where[0] ) ; i++ ) {
		put ( "SPIKE claim@" );
		puthex ( where[i] );
		put ( " 1MiB -> " );
		r = claim ( where[i], 0x100000, 0 );
		puthex ( r );
		put ( "\r\n" );
		if ( r == where[i] ) {
			volatile u64 *p = ( volatile u64 * ) ( u64 ) r;
			*p = 0x5a5aa5a512345678UL;
			kv ( "  real-mode store/load round trip ok",
			     *p == 0x5a5aa5a512345678UL );
			release ( where[i], 0x100000 );
		}
	}
	r = claim ( 0, 0x100000, 0x1000 );
	kv ( "claim(any,1MiB,4KiB)", r );
	if ( r != 0xffffffff )
		release ( r, 0x100000 );
	r = claim ( 0, 0x10000000, 0x1000 );
	kv ( "claim(any,256MiB,4KiB)", r );
	if ( r != 0xffffffff )
		release ( r, 0x10000000 );
	r = claim ( 0, 0x40000000, 0x1000 );
	kv ( "claim(any,1GiB,4KiB)", r );
	if ( r != 0xffffffff )
		release ( r, 0x40000000 );
}

/** Spin for approximately the given number of timebase ticks */
static void spin ( u64 ticks ) {
	u64 start, now;
	__asm__ volatile ( "mftb %0" : "=r" ( start ) );
	do {
		__asm__ volatile ( "mftb %0" : "=r" ( now ) );
	} while ( ( now - start ) < ticks );
}

static void probe_net ( void ) {
	static u8 mac[6], frame[64], rx[1600];
	static char path[96];
	u32 node = finddevice ( "net" );
	u32 aliases = finddevice ( "/aliases" );
	u32 ih, i, n = 0;
	int got = -1;

	memset ( path, 0, sizeof ( path ) );
	getprop ( aliases, "net", path, sizeof ( path ) - 1 );
	say ( "net alias path", path );
	kv ( "net node phandle", node );
	getprop ( node, "local-mac-address", mac, 6 );
	put ( "SPIKE net local-mac-address =" );
	for ( i = 0 ; i < 6 ; i++ ) {
		put ( " " );
		puthex ( mac[i] );
	}
	put ( "\r\n" );
	{ u32 in[1] = { A ( "net" ) }; ih = ci ( "open", 1, 1, in, 0 ); }
	kv ( "net open ihandle", ih );
	if ( ! ih )
		return;
	memset ( frame, 0, sizeof ( frame ) );
	for ( i = 0 ; i < 6 ; i++ ) {
		frame[i] = 0xff;
		frame[6 + i] = mac[i];
	}
	frame[12] = 0x08; frame[13] = 0x06;	/* ARP */
	frame[15] = 1; frame[16] = 0x08; frame[18] = 6; frame[19] = 4;
	frame[21] = 1;				/* request */
	for ( i = 0 ; i < 6 ; i++ )
		frame[22 + i] = mac[i];
	frame[28] = 10; frame[30] = 2; frame[31] = 15;	/* 10.0.2.15 */
	frame[38] = 10; frame[40] = 2; frame[41] = 2;	/* 10.0.2.2 */
	{
		u32 in[3] = { ih, A ( frame ), 60 };
		kv ( "net write(60) returned", ci ( "write", 3, 1, in, 0 ) );
	}
	for ( i = 0 ; i < 40 && got < 0 ; i++ ) {
		u32 in[3] = { ih, A ( rx ), sizeof ( rx ) };
		int len = ( int ) ci ( "read", 3, 1, in, 0 );
		if ( len > 0 ) {
			n++;
			kv ( "net read frame length", len );
			kv ( "net rx ethertype",
			     ( rx[12] << 8 ) | rx[13] );
			kv ( "net rx arp opcode", rx[21] );
			if ( rx[12] == 8 && rx[13] == 6 && rx[21] == 2 )
				got = len;
		} else {
			spin ( 50000000 );
		}
	}
	kv ( "net arp reply received", got > 0 );
	{
		/* The ARP reply was the only frame: this read finds none */
		u32 in[3] = { ih, A ( rx ), sizeof ( rx ) };
		kv ( "net read with no frame waiting returned",
		     ci ( "read", 3, 1, in, 0 ) );
	}
	if ( got > 0 ) {
		put ( "SPIKE net arp reply sender mac =" );
		for ( i = 0 ; i < 6 ; i++ ) {
			put ( " " );
			puthex ( rx[22 + i] );
		}
		put ( "\r\n" );
	}
	{ u32 in[1] = { ih }; ci ( "close", 1, 0, in, 0 ); }
}

/** RTAS argument block (32-bit big-endian cells) */
struct rtas_args {
	u32 token, nargs, nret;
	u32 cell[8];
};

static u32 rtas_instantiate ( u32 rtas_ih, u32 base, u32 *entry ) {
	u32 in[3] = { A ( "instantiate-rtas" ), rtas_ih, base };
	u32 out[2] = { 0xdead, 0xdead };
	ci ( "call-method", 3, 2, in, out );
	*entry = out[1];
	return out[0];
}

static void rtas_time ( const char *label, u32 entry, u32 base, u32 token ) {
	static struct rtas_args args;
	u32 i;

	memset ( &args, 0, sizeof ( args ) );
	args.token = bswap32 ( token );
	args.nargs = 0;
	args.nret = bswap32 ( 8 );
	be_call ( entry, ( u64 ) &args, base, 0 );
	put ( "SPIKE " );
	put ( label );
	put ( " get-time-of-day ret[status,y,m,d,h,m,s,ns] =" );
	for ( i = 0 ; i < 8 ; i++ ) {
		put ( " " );
		puthex ( bswap32 ( args.cell[i] ) );
	}
	put ( "\r\n" );
}

static void probe_rtas ( void ) {
	u32 node = finddevice ( "/rtas" );
	u32 size = prop32 ( node, "rtas-size" );
	u32 token = prop32 ( node, "get-time-of-day" );
	u32 ih, base1, base2, e1 = 0, e2 = 0, st1, st2;
	u32 in[1] = { A ( "/rtas" ) };

	kv ( "rtas node phandle", node );
	kv ( "rtas-size", size );
	kv ( "rtas get-time-of-day token", token );
	ih = ci ( "open", 1, 1, in, 0 );
	kv ( "rtas open ihandle", ih );
	base1 = claim ( 0, size, 0x1000 );
	kv ( "rtas claim base1", base1 );
	st1 = rtas_instantiate ( ih, base1, &e1 );
	kv ( "rtas instantiate #1 status", st1 );
	kv ( "rtas instantiate #1 entry", e1 );
	rtas_time ( "rtas#1", e1, base1, token );
	base2 = claim ( 0, size, 0x1000 );
	kv ( "rtas claim base2", base2 );
	st2 = rtas_instantiate ( ih, base2, &e2 );
	kv ( "rtas instantiate #2 (new base) status", st2 );
	kv ( "rtas instantiate #2 (new base) entry", e2 );
	rtas_time ( "rtas#2", e2, base2, token );
	rtas_time ( "rtas#1 again", e1, base1, token );
	st2 = rtas_instantiate ( ih, base1, &e2 );
	kv ( "rtas instantiate #3 (same base) status", st2 );
	kv ( "rtas instantiate #3 (same base) entry", e2 );
}

void probe_main ( void ) {
	u32 chosen;

	/* Locate the console before anything can be printed */
	chosen = finddevice ( "/chosen" );
	getprop ( chosen, "stdout", &stdout_ih, 4 );
	stdout_ih = bswap32 ( stdout_ih );
	put ( "SPIKE begin\r\n" );
	probe_entry();
	probe_tree();
	probe_claim();
	probe_net();
	probe_rtas();
	put ( "SPIKE done\r\n" );
	ci ( "exit", 0, 0, 0, 0 );
}
