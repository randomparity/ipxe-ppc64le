#ifndef CONFIG_DEFAULTS_IEEE1275_H
#define CONFIG_DEFAULTS_IEEE1275_H

/** @file
 *
 * Configuration defaults for IEEE 1275 Open Firmware (pSeries)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/* The firmware runs the client in real mode: virtual equals physical */
#define UACCESS_FLAT
#define DMAAPI_FLAT

#define TIMER_PPC64LE

#define MEMMAP_IEEE1275
#define UMALLOC_UHEAP

/* Providers that await RTAS and entropy support */
#define TIME_NULL
#define REBOOT_NULL

#define ACPI_NULL
#define FDT_NULL
#define MPAPI_NULL
#define NAP_NULL
#define PCIAPI_NULL
#define SANBOOT_NULL
#define SERIAL_NULL
#define SMBIOS_NULL

#endif /* CONFIG_DEFAULTS_IEEE1275_H */
