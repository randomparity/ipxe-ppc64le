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
 * Open Firmware network devices
 *
 * Each network node in the device tree is presented as an Ethernet
 * device that sends and receives raw frames through the client
 * interface @c write and @c read calls.  The firmware offers neither
 * link state nor interrupts, so the device is polled.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/list.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/device.h>
#include <ipxe/ieee1275.h>

/** Maximum depth of the device tree walk */
#define IEEE1275_NET_MAX_DEPTH 16

/** Maximum length of a device path (including terminator) */
#define IEEE1275_NET_PATH_LEN 256

/** Maximum length of a device type (including terminator) */
#define IEEE1275_NET_TYPE_LEN 16

/** Maximum number of frames received per poll */
#define IEEE1275_NET_RX_QUOTA 8

/** An Open Firmware network device */
struct ieee1275_nic {
	/** Network device */
	struct net_device *netdev;
	/** Generic device */
	struct device dev;
	/** Package handle */
	ieee1275_phandle_t phandle;
	/** Instance handle (zero if not open) */
	ieee1275_ihandle_t ihandle;
	/** Receive buffer (retained between polls) */
	struct io_buffer *rxbuf;
};

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ieee1275_net_transmit ( struct net_device *netdev,
				   struct io_buffer *iobuf ) {
	struct ieee1275_nic *nic = netdev->priv;
	int len;

	/* Pad to minimum Ethernet length */
	iob_pad ( iobuf, ETH_ZLEN );

	/* The firmware sends synchronously */
	len = ieee1275_write ( nic->ihandle, iobuf->data, iob_len ( iobuf ) );
	if ( len != ( ( int ) iob_len ( iobuf ) ) ) {
		DBGC ( nic, "IEEE1275 %s could not transmit %zd bytes: %s\n",
		       netdev->name, iob_len ( iobuf ),
		       strerror ( ( len < 0 ) ? len : -EIO ) );
		return ( ( len < 0 ) ? len : -EIO );
	}
	netdev_tx_complete ( netdev, iobuf );

	return 0;
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void ieee1275_net_poll ( struct net_device *netdev ) {
	struct ieee1275_nic *nic = netdev->priv;
	unsigned int quota;
	int len;

	for ( quota = IEEE1275_NET_RX_QUOTA ; quota ; quota-- ) {

		/* Allocate buffer, if required */
		if ( ! nic->rxbuf )
			nic->rxbuf = alloc_iob ( netdev->max_pkt_len );
		if ( ! nic->rxbuf )
			return;

		/* Receive frame; ieee1275_read() bounds the length
		 * by the buffer size.  Zero means no frame is waiting.
		 */
		len = ieee1275_read ( nic->ihandle, nic->rxbuf->data,
				      iob_tailroom ( nic->rxbuf ) );
		if ( len == 0 )
			return;
		if ( len < 0 ) {
			DBGC ( nic, "IEEE1275 %s could not receive: %s\n",
			       netdev->name, strerror ( len ) );
			netdev_rx_err ( netdev, NULL, len );
			return;
		}

		/* Hand off to network stack */
		iob_put ( nic->rxbuf, len );
		netdev_rx ( netdev, nic->rxbuf );
		nic->rxbuf = NULL;
	}
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ieee1275_net_open ( struct net_device *netdev ) {
	struct ieee1275_nic *nic = netdev->priv;
	char path[IEEE1275_NET_PATH_LEN];
	int rc;

	/* Open the firmware instance */
	if ( ( rc = ieee1275_package_to_path ( nic->phandle, path,
					       sizeof ( path ) ) ) != 0 ) {
		DBGC ( nic, "IEEE1275 %s has no usable path: %s\n",
		       netdev->name, strerror ( rc ) );
		return rc;
	}
	if ( ( rc = ieee1275_open ( path, &nic->ihandle ) ) != 0 ) {
		DBGC ( nic, "IEEE1275 %s could not open %s: %s\n",
		       netdev->name, path, strerror ( rc ) );
		nic->ihandle = 0;
		return rc;
	}
	DBGC ( nic, "IEEE1275 %s opened %s\n", netdev->name, path );

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void ieee1275_net_close ( struct net_device *netdev ) {
	struct ieee1275_nic *nic = netdev->priv;

	/* Close the firmware instance */
	if ( nic->ihandle ) {
		ieee1275_close ( nic->ihandle );
		nic->ihandle = 0;
		DBGC ( nic, "IEEE1275 %s closed\n", netdev->name );
	}

	/* Discard any retained receive buffer */
	free_iob ( nic->rxbuf );
	nic->rxbuf = NULL;
}

/** Open Firmware network device operations */
static struct net_device_operations ieee1275_net_operations = {
	.open		= ieee1275_net_open,
	.close		= ieee1275_net_close,
	.transmit	= ieee1275_net_transmit,
	.poll		= ieee1275_net_poll,
};

/**
 * Create network device for a network node
 *
 * @v rootdev		Root device
 * @v phandle		Package handle
 * @ret rc		Return status code
 */
static int ieee1275_net_add ( struct root_device *rootdev,
			      ieee1275_phandle_t phandle ) {
	struct net_device *netdev;
	struct ieee1275_nic *nic;
	size_t size;
	int rc;

	/* Allocate and initialise structure */
	netdev = alloc_etherdev ( sizeof ( *nic ) );
	if ( ! netdev )
		return -ENOMEM;
	netdev_init ( netdev, &ieee1275_net_operations );
	nic = netdev->priv;
	nic->netdev = netdev;
	nic->phandle = phandle;
	snprintf ( nic->dev.name, sizeof ( nic->dev.name ), "pkg%#x",
		   phandle );
	nic->dev.driver_name = "ieee1275";
	nic->dev.parent = &rootdev->dev;
	list_add_tail ( &nic->dev.siblings, &rootdev->dev.children );
	INIT_LIST_HEAD ( &nic->dev.children );
	netdev->dev = &nic->dev;

	/* Read the MAC address, which must be exactly an Ethernet address */
	if ( ( rc = ieee1275_getprop ( phandle, "local-mac-address",
				       netdev->hw_addr, ETH_ALEN,
				       &size ) ) != 0 )
		goto err_mac;
	if ( size != ETH_ALEN ) {
		rc = -EINVAL;
		goto err_mac;
	}

	/* Register network device; the firmware reports no link state */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;
	netdev_link_up ( netdev );
	DBGC ( nic, "IEEE1275 %s is package %#x\n", netdev->name, phandle );

	return 0;

	unregister_netdev ( netdev );
 err_register:
 err_mac:
	list_del ( &nic->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
	return rc;
}

/**
 * Check whether a package is a network node
 *
 * @v phandle		Package handle
 * @ret is_net		Package is a network node
 */
static int ieee1275_net_is_net ( ieee1275_phandle_t phandle ) {
	static const char network[] = "network";
	char type[IEEE1275_NET_TYPE_LEN];
	size_t size;

	/* Reject any value that does not fit with its terminator */
	memset ( type, 0, sizeof ( type ) );
	if ( ieee1275_getprop ( phandle, "device_type", type,
				( sizeof ( type ) - 1 ), &size ) != 0 )
		return 0;
	return ( ( size == sizeof ( network ) ) &&
		 ( memcmp ( type, network, sizeof ( network ) ) == 0 ) );
}

/**
 * Walk a device tree, adding a network device for each network node
 *
 * @v rootdev		Root device
 * @v phandle		Package handle of first node at this level
 * @v depth		Remaining depth
 */
static void ieee1275_net_walk ( struct root_device *rootdev,
				ieee1275_phandle_t phandle,
				unsigned int depth ) {
	ieee1275_phandle_t child;
	int rc;

	/* Visit this node and each of its peers, then their children */
	do {
		if ( ieee1275_net_is_net ( phandle ) &&
		     ( ( rc = ieee1275_net_add ( rootdev, phandle ) ) != 0 ) ) {
			DBGC ( rootdev, "IEEE1275 package %#x skipped: %s\n",
			       phandle, strerror ( rc ) );
		}
		if ( depth && ( ieee1275_child ( phandle, &child ) == 0 ) )
			ieee1275_net_walk ( rootdev, child, ( depth - 1 ) );
	} while ( ieee1275_peer ( phandle, &phandle ) == 0 );
}

/**
 * Probe root device
 *
 * @v rootdev		Root device
 * @ret rc		Return status code
 *
 * A node that cannot be added is skipped, so that this succeeds
 * whenever the device tree could be walked: the removal function
 * then always runs, and closes every device that was added.
 */
static int ieee1275_net_probe ( struct root_device *rootdev ) {
	ieee1275_phandle_t root;
	int rc;

	if ( ( rc = ieee1275_finddevice ( "/", &root ) ) != 0 )
		return rc;
	ieee1275_net_walk ( rootdev, root, IEEE1275_NET_MAX_DEPTH );

	return 0;
}

/**
 * Remove root device
 *
 * @v rootdev		Root device
 */
static void ieee1275_net_remove ( struct root_device *rootdev ) {
	struct ieee1275_nic *nic;
	struct ieee1275_nic *tmp;
	struct net_device *netdev;

	list_for_each_entry_safe ( nic, tmp, &rootdev->dev.children,
				   dev.siblings ) {
		netdev = nic->netdev;
		unregister_netdev ( netdev );
		list_del ( &nic->dev.siblings );
		netdev_nullify ( netdev );
		netdev_put ( netdev );
	}
}

/** Open Firmware network root device driver */
static struct root_driver ieee1275_net_driver = {
	.probe = ieee1275_net_probe,
	.remove = ieee1275_net_remove,
};

/** Open Firmware network root device */
struct root_device ieee1275_net_root_device __root_device = {
	.dev = { .name = "ieee1275" },
	.driver = &ieee1275_net_driver,
};
