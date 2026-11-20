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

/** Value returned by the firmware for "no such package" or "failed" */
#define NONE 0xffffffffU

static u32 stdout_ih;

/** Real-mode area: size of the memory node whose base is zero */
static u64 rma_size;

/** First network node found by the tree walk */
static u32 net_ph;
static char net_path[128];

/** Timebase ticks per second (from the first CPU node) */
static u64 tb_freq = 512000000;

static int cpu_seen;

static u32 bswap32 ( u32 v ) { return __builtin_bswap32 ( v ); }

static u64 strlen ( const char *s ) {
	u64 n = 0;
	while ( s[n] )
		n++;
	return n;
}

static int streq ( const char *a, const char *b ) {
	while ( *a && ( *a == *b ) ) {
		a++;
		b++;
	}
	return ( *a == *b );
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

static int getproplen ( u32 ph, const char *name ) {
	u32 in[2] = { ph, A ( name ) };
	return ( int ) ci ( "getproplen", 2, 1, in, 0 );
}

static u32 prop32 ( u32 ph, const char *name ) {
	u32 v = 0;
	getprop ( ph, name, &v, 4 );
	return bswap32 ( v );
}

static u32 child ( u32 ph ) {
	u32 in[1] = { ph };
	return ci ( "child", 1, 1, in, 0 );
}

static u32 peer ( u32 ph ) {
	u32 in[1] = { ph };
	return ci ( "peer", 1, 1, in, 0 );
}

static int is_node ( u32 ph ) {
	return ( ( ph != 0 ) && ( ph != NONE ) );
}

static u64 mfmsr ( void ) {
	u64 msr;
	__asm__ volatile ( "mfmsr %0" : "=r" ( msr ) );
	return msr;
}

static u64 mftb ( void ) {
	u64 tb;
	__asm__ volatile ( "mftb %0" : "=r" ( tb ) );
	return tb;
}

/** Read big-endian cells as one value */
static u64 cells ( const u8 *p, u32 count ) {
	u64 v = 0;
	u32 i;
	for ( i = 0 ; i < ( count * 4 ) ; i++ )
		v = ( ( v << 8 ) | p[i] );
	return v;
}

/** Print a 32-bit property, or that it is absent */
static void kvprop32 ( const char *key, u32 ph, const char *name ) {
	if ( getproplen ( ph, name ) == 4 ) {
		kv ( key, prop32 ( ph, name ) );
	} else {
		say ( key, "absent" );
	}
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
	static u8 buf[128];
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
	if ( len > ( int ) sizeof ( buf ) )
		put ( "..." );
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

/** Print a memory node; note the real-mode area */
static void node_memory ( u32 ph, u32 acells, u32 scells ) {
	static u8 reg[16];
	int len = getprop ( ph, "reg", reg, sizeof ( reg ) );

	hexdump_prop ( "  reg", ph, "reg" );
	hexdump_prop ( "  available", ph, "available" );
	if ( ( acells + scells ) > 4 )
		return;
	if ( ( len >= ( int ) ( ( acells + scells ) * 4 ) ) &&
	     ( cells ( reg, acells ) == 0 ) )
		rma_size = cells ( ( reg + ( acells * 4 ) ), scells );
}

/** Print a network node; remember the first */
static void node_network ( u32 ph, const char *path ) {
	static char text[64];
	u64 i;

	memset ( text, 0, sizeof ( text ) );
	getprop ( ph, "compatible", text, ( sizeof ( text ) - 1 ) );
	say ( "  compatible", text );
	list_props ( "network node", ph );
	if ( net_ph )
		return;
	net_ph = ph;
	for ( i = 0 ; path[i] ; i++ )
		net_path[i] = path[i];
}

/** Print the first CPU node */
static void node_cpu ( u32 ph ) {
	if ( cpu_seen++ )
		return;
	kvprop32 ( "  cpu-version", ph, "cpu-version" );
	kvprop32 ( "  timebase-frequency", ph, "timebase-frequency" );
	if ( getproplen ( ph, "timebase-frequency" ) == 4 )
		tb_freq = prop32 ( ph, "timebase-frequency" );
}

/** Walk the device tree, printing memory, network and CPU nodes */
static void walk ( u32 ph, u32 acells, u32 scells, unsigned depth ) {
	static char type[32], path[128];

	for ( ; is_node ( ph ) ; ph = peer ( ph ) ) {
		memset ( type, 0, sizeof ( type ) );
		getprop ( ph, "device_type", type, ( sizeof ( type ) - 1 ) );
		if ( streq ( type, "memory" ) || streq ( type, "network" ) ||
		     streq ( type, "cpu" ) ) {
			u32 in[3] = { ph, A ( path ), ( sizeof ( path ) - 1 ) };
			memset ( path, 0, sizeof ( path ) );
			ci ( "package-to-path", 3, 1, in, 0 );
			put ( "SPIKE node " );
			put ( type );
			put ( " " );
			put ( path );
			put ( "\r\n" );
			if ( streq ( type, "memory" ) ) {
				node_memory ( ph, acells, scells );
			} else if ( streq ( type, "network" ) ) {
				node_network ( ph, path );
			} else {
				node_cpu ( ph );
			}
		}
		if ( depth )
			walk ( child ( ph ), acells, scells, ( depth - 1 ) );
	}
}

/** List the property names of a node that may be absent */
static void list_optional ( const char *path ) {
	u32 ph = finddevice ( path );

	if ( is_node ( ph ) ) {
		list_props ( path, ph );
	} else {
		say ( path, "absent" );
	}
}

static void probe_tree ( void ) {
	static char text[128];
	u32 root = finddevice ( "/" );
	u32 chosen = finddevice ( "/chosen" );
	u32 acells = 2, scells = 1;

	list_props ( "/", root );
	list_props ( "/chosen", chosen );
	if ( getproplen ( root, "#address-cells" ) == 4 )
		acells = prop32 ( root, "#address-cells" );
	if ( getproplen ( root, "#size-cells" ) == 4 )
		scells = prop32 ( root, "#size-cells" );
	kv ( "/ #address-cells", acells );
	kv ( "/ #size-cells", scells );
	walk ( child ( root ), acells, scells, 6 );
	kv ( "real-mode area size (memory node at 0)", rma_size );
	list_optional ( "/ibm,dynamic-reconfiguration-memory" );
	list_optional ( "/rtas" );
	memset ( text, 0, sizeof ( text ) );
	getprop ( finddevice ( "/openprom" ), "model", text,
		  ( sizeof ( text ) - 1 ) );
	say ( "/openprom model", text );
	kv ( "/chosen bootargs length",
	     ( u64 ) ( long ) getproplen ( chosen, "bootargs" ) );
	kv ( "/chosen bootpath length",
	     ( u64 ) ( long ) getproplen ( chosen, "bootpath" ) );
	hexdump_prop ( "/chosen linux,initrd-start", chosen,
		       "linux,initrd-start" );
}

/** Claim 1MiB at a fixed address; touch it only inside the real-mode area */
static void claim_at ( u32 addr ) {
	u32 r;

	put ( "SPIKE claim@" );
	puthex ( addr );
	put ( " 1MiB -> " );
	r = claim ( addr, 0x100000, 0 );
	puthex ( r );
	put ( "\r\n" );
	if ( r != addr )
		return;
	if ( ( ( u64 ) addr + 0x100000 ) <= rma_size ) {
		volatile u64 *p = ( volatile u64 * ) ( u64 ) r;
		*p = 0x5a5aa5a512345678UL;
		kv ( "  real-mode store/load round trip ok",
		     *p == 0x5a5aa5a512345678UL );
	} else {
		say ( "  claimed above the real-mode area", "not dereferenced" );
	}
	release ( addr, 0x100000 );
}

static void probe_claim ( void ) {
	static const u32 where[] = {
		0x1000000, 0x10000000, 0x20000000, 0x30000000, 0x3ff00000,
		0x40000000, 0x7ff00000, 0xbff00000, 0xfff00000
	};
	u32 r;
	unsigned i;

	for ( i = 0 ; i < sizeof ( where ) / sizeof ( where[0] ) ; i++ )
		claim_at ( where[i] );
	if ( ( rma_size >= 0x200000 ) && ( rma_size < 0x100000000UL ) ) {
		put ( "SPIKE next two claims: top of the real-mode area, "
		      "then just above it\r\n" );
		claim_at ( rma_size - 0x100000 );
		claim_at ( rma_size );
	}
	r = claim ( 0, 0x100000, 0x1000 );
	kv ( "claim(any,1MiB,4KiB)", r );
	if ( r != NONE )
		release ( r, 0x100000 );
	r = claim ( 0, 0x10000000, 0x1000 );
	kv ( "claim(any,256MiB,4KiB)", r );
	if ( r != NONE )
		release ( r, 0x10000000 );
	r = claim ( 0, 0x40000000, 0x1000 );
	kv ( "claim(any,1GiB,4KiB)", r );
	if ( r != NONE )
		release ( r, 0x40000000 );
}

/** Spin for approximately the given number of timebase ticks */
static void spin ( u64 ticks ) {
	u64 start = mftb();

	while ( ( mftb() - start ) < ticks ) {
		/* Nothing */
	}
}

/** Network probe state */
struct net_probe {
	u32 ih;
	u8 mac[6];
	u8 target[4];
	unsigned frames;
	unsigned empty;
	int last_empty;
	int learned;
	int replied;
};

/**
 * Read frames for a while
 *
 * Before the request is sent, the sender of any ARP packet seen
 * becomes the target (a host known to be present).  Afterwards, an
 * ARP reply addressed to this node ends the wait.
 */
static void net_listen ( struct net_probe *net, u64 ticks, int sent ) {
	static u8 rx[1600];
	u64 start = mftb();
	int len, i, ours;

	net->frames = 0;
	while ( ( ( mftb() - start ) < ticks ) && ( ! net->replied ) ) {
		u32 in[3] = { net->ih, A ( rx ), sizeof ( rx ) };
		len = ( int ) ci ( "read", 3, 1, in, 0 );
		if ( len <= 0 ) {
			net->empty++;
			net->last_empty = len;
			spin ( tb_freq / 100 );
			continue;
		}
		net->frames++;
		if ( ( len < 42 ) || ( rx[12] != 0x08 ) || ( rx[13] != 0x06 ) )
			continue;
		for ( ours = 1, i = 0 ; i < 6 ; i++ ) {
			if ( rx[( sent ? 32 : 22 ) + i] != net->mac[i] )
				ours = 0;
		}
		if ( sent ) {
			net->replied = ( ours && ( rx[21] == 2 ) );
		} else if ( ( ! ours ) && ( ! net->learned ) &&
			    ( rx[28] | rx[29] | rx[30] | rx[31] ) ) {
			for ( i = 0 ; i < 4 ; i++ )
				net->target[i] = rx[28 + i];
			net->learned = 1;
		}
	}
}

static void probe_net ( void ) {
	static struct net_probe net;
	static u8 frame[64];
	u32 alias = finddevice ( "net" );
	u32 node = ( is_node ( alias ) ? alias : net_ph );
	u32 i;

	kv ( "net alias present", is_node ( alias ) );
	if ( ! is_node ( node ) ) {
		say ( "net", "no network node" );
		return;
	}
	kv ( "net local-mac-address length", ( u64 ) ( long )
	     getprop ( node, "local-mac-address", net.mac, 6 ) );
	kv ( "net locally administered bit", ( net.mac[0] >> 1 ) & 1 );
	{
		u32 in[1] = { A ( is_node ( alias ) ? "net" : net_path ) };
		net.ih = ci ( "open", 1, 1, in, 0 );
	}
	kv ( "net open ihandle non-zero", ( net.ih != 0 ) );
	if ( ! net.ih )
		return;

	/* Fallback target: the QEMU user-mode network gateway */
	net.target[0] = 10;
	net.target[2] = 2;
	net.target[3] = 2;
	net_listen ( &net, ( 2 * tb_freq ), 0 );
	kv ( "net frames seen before send", net.frames );
	kv ( "net arp target learned from traffic", net.learned );

	/* ARP probe: sender address 0.0.0.0, so no host caches it */
	memset ( frame, 0, sizeof ( frame ) );
	for ( i = 0 ; i < 6 ; i++ ) {
		frame[i] = 0xff;
		frame[6 + i] = net.mac[i];
		frame[22 + i] = net.mac[i];
	}
	frame[12] = 0x08; frame[13] = 0x06;	/* ARP */
	frame[15] = 1; frame[16] = 0x08; frame[18] = 6; frame[19] = 4;
	frame[21] = 1;				/* request */
	for ( i = 0 ; i < 4 ; i++ )
		frame[38 + i] = net.target[i];
	{
		u32 in[3] = { net.ih, A ( frame ), 60 };
		kv ( "net write(60) returned", ci ( "write", 3, 1, in, 0 ) );
	}
	net_listen ( &net, ( 4 * tb_freq ), 1 );
	kv ( "net frames seen after send", net.frames );
	kv ( "net arp reply received", net.replied );
	kv ( "net reads that returned no frame", net.empty );
	kv ( "net last no-frame read returned",
	     ( u64 ) ( long ) net.last_empty );
	{ u32 in[1] = { net.ih }; ci ( "close", 1, 0, in, 0 ); }
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

/**
 * Instantiate RTAS at a base and read the time through it
 *
 * @ret entry		RTAS entry point, or 0 if it cannot be called
 */
static u32 rtas_try ( const char *label, u32 ih, u32 base, u32 token ) {
	u32 entry = 0, status;

	put ( "SPIKE " );
	put ( label );
	put ( " base = " );
	puthex ( base );
	put ( "\r\n" );
	if ( base == NONE ) {
		say ( label, "skipped (no base)" );
		return 0;
	}
	status = rtas_instantiate ( ih, base, &entry );
	put ( "SPIKE " );
	put ( label );
	put ( " instantiate status, entry = " );
	puthex ( status );
	put ( " " );
	puthex ( entry );
	put ( "\r\n" );
	if ( ( status != 0 ) || ( entry == 0 ) || ( entry == 0xdead ) ) {
		say ( label, "call skipped (instantiate failed)" );
		return 0;
	}
	rtas_time ( label, entry, base, token );
	return entry;
}

static void probe_rtas ( void ) {
	u32 node = finddevice ( "/rtas" );
	u32 size = prop32 ( node, "rtas-size" );
	u32 token = prop32 ( node, "get-time-of-day" );
	u32 ih, base1, e1;
	u32 in[1] = { A ( "/rtas" ) };

	kv ( "rtas node present", is_node ( node ) );
	kv ( "rtas-size", size );
	kv ( "rtas get-time-of-day token", token );
	ih = ci ( "open", 1, 1, in, 0 );
	kv ( "rtas open ihandle non-zero", ( ih != 0 ) );
	if ( ( ! ih ) || ( ! size ) ) {
		say ( "rtas", "skipped (no instance or size)" );
		return;
	}
	base1 = claim ( 0, size, 0x1000 );
	e1 = rtas_try ( "rtas#1", ih, base1, token );
	rtas_try ( "rtas#2 (new base)", ih, claim ( 0, size, 0x1000 ), token );
	if ( e1 )
		rtas_time ( "rtas#1 again", e1, base1, token );
	rtas_try ( "rtas#3 (same base as #1)", ih, base1, token );
}

/** Execute DARN: on a processor or mode without it, this traps */
static void probe_darn ( void ) {
	register u64 value __asm__ ( "r3" );

	put ( "SPIKE darn = executing (a trap ends the run here)\r\n" );
	__asm__ volatile ( ".long 0x7c6105e6" /* darn r3, 1 */
			   : "=r" ( value ) );
	kv ( "darn returned its failure value (all ones)", ( value == ~0UL ) );
}

void probe_main ( void ) {
	u32 chosen;

	/* Locate the console before anything can be printed */
	chosen = finddevice ( "/chosen" );
	getprop ( chosen, "stdout", &stdout_ih, 4 );
	stdout_ih = bswap32 ( stdout_ih );
	put ( "SPIKE begin\r\n" );
	kv ( "msr after first calls", mfmsr() );
	probe_entry();
	probe_tree();
	probe_claim();
	probe_net();
	probe_rtas();
	kv ( "msr before darn", mfmsr() );
	probe_darn();
	put ( "SPIKE done\r\n" );
	ci ( "exit", 0, 0, 0, 0 );
}
