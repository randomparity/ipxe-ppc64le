# ppc64le firmware probe record: PowerVM partition firmware

The firmware spike (`../ppc64le-spike`) and the iPXE ISO were booted from
virtual optical media on a PowerVM logical partition. This record lists
what differs from the SLOF record in `../ppc64le-spike/README.md`, in that
record's order, and which differences need code.

Everything below was observed on the partition console unless it says
otherwise. Console text is excerpted, with the firmware's screen drawing
removed and elisions marked `[...]`. Redacted: the MAC address and the
location codes that the firmware prints (shown as `<mac>` and `<loc>`).
Host, system and partition names, addresses, serial numbers and
identifiers of the test environment are not recorded. The probe itself
prints no MAC or IP address.

## Inputs

| Item | Value |
|---|---|
| Partition | PowerVM LPAR, POWER9 (PVR `0x004e0202`), `cpu-version` `0x0f000005`, 4 GiB, 0.5 processing units, one virtual processor, one virtual Ethernet adapter, one virtual SCSI adapter for the optical device (and, on the second partition, one for an unused disk) |
| Firmware | `/openprom` `model`: `IBM,FW950.00 (VL950_039)`; secure boot off |
| Probe | `contrib/ppc64le-spike` at `b7acf7090`, built with `make CROSS=powerpc64le-linux-gnu- RTAS_CALLS=0` (GCC 14.2.0) except for the first RTAS attempt (see "RTAS") |
| Probe media | `./run.sh iso`: one ISO per variant, plain ISO9660 with Rock Ridge, `/ppc/bootinfo.txt` and `/ppc/spike.elf` |
| iPXE | `make -C src bin-ppc64le-ieee1275/ipxe.iso CROSS_COMPILE=powerpc64le-linux-gnu- EMBED=../contrib/ppc64le/installer-menu.ipxe`, source as `ppc64le-support` at `8e9744fff`, with one local change (see "Deviations") |
| Boot | The ISO loaded into the partition's virtual optical device, that device as the only entry of the boot list, normal-mode activation; some runs instead from the firmware prompt (see "Firmware prompt") |
| Console | captured through the management console; no key was sent to a probe or to iPXE |
| Media identity | the name of the image loaded in the optical device was read back before each activation; the console does not show it (the probe as run printed neither `built with` line) |

Two partitions of the same shape were used, 17 activations in all: six on
the first, eleven on its replacement. Eight of them tested file naming and
hypotheses that turned out wrong (see "ELF acceptance").

## Deviations

- After the first RTAS attempt (see "RTAS"), no image containing RTAS
  code was run. The probe was built with `RTAS_CALLS=0`. The iPXE image was
  built with `config/local/time.h` and `config/local/reboot.h` replacing
  `TIME_RTAS` and `REBOOT_RTAS` by `TIME_NULL` and `REBOOT_NULL`, so that
  `ieee1275_rtas.c` is not linked. The ISO is otherwise what the command
  above produces. The image proven on QEMU, with the RTAS providers, was
  not run. This was a precaution: read against the code, that image would
  have refused to instantiate RTAS here (see "RTAS").
- The `be32` variant was run before the probe's `.bss` handling and network
  open order were corrected; its network and `.bss` results are not used.

## Differences from SLOF

| Area | SLOF | PowerVM firmware | Needs code |
|---|---|---|---|
| ELF acceptance | all five variants load and run | `le64v2`, `le32`, `be32` load and run; `le64v1` and `be64` trap at entry | no: iPXE is `le64v2` |
| File name in `bootinfo.txt` | plain ISO9660 names only | the same, as far as tested | no: not a difference; `ipxe.elf` is found |
| Entry state, `le64v2` | MSR `0x8000000000003001`, `r12 = r13 =` entry | MSR `0x8000000000003003` (RI also set), `r12 =` entry, `r13 = 0` | no |
| `.bss` | zero at entry | not zeroed | no: `elfprefix.S` zeroes it |
| Real-mode area | first memory node covers all RAM; `claim` works anywhere in RAM | 256 MiB; the largest free range is about 109 MiB; nothing above it can be claimed | yes: #38 (memory provider) |
| Network node `open` | alias opens (iPXE opens the path) | plain path or alias runs BOOTP and fails; opens with arguments | yes: #39 (network driver) |
| Raw frames | `write`, non-blocking `read` | the same, once open | no |
| RTAS size | `rtas-size` `0x82c` | `rtas-size` `0x1360000`, above iPXE's 1 MiB limit | yes: #41 |
| RTAS calls | instantiates repeatedly; time of day works | not established: the one attempt ended the run | investigation: #40 |
| DARN | returns a value (QEMU default CPU) | returns a value in the mode above | no; see #32 |

Epic R8 confines PowerVM fixes to the prefix, the linker script and the
memory provider. #38 fits (memory provider). #39 and #41 do not: the
changes belong in the network driver and in the RTAS module. Nothing was
found that needs the prefix or the linker script to change.

### ELF acceptance

Booted through `/ppc/bootinfo.txt`, each from its own ISO, in normal mode
except `be32`, which was started from the firmware prompt:

| Variant | ELF class / data | `e_entry` | Result |
|---|---|---|---|
| `le64v2` | 64, little, ELFv2 | code | loaded at `0x400000`; entered little-endian, 64-bit |
| `le64v1` | 64, little, ELFv1 | descriptor | entered at `e_entry` itself, big-endian and 32-bit: program exception |
| `be64` | 64, big | descriptor | the same |
| `le32` | 32, little | code | entered big-endian, 32-bit (MSR `0x3002`); the stub switches; ran to the end |
| `be32` | 32, big | code | entered big-endian, 32-bit (MSR `0x3002`); ran to the end |
| `sf0` | as `le64v2` | code | as `le64v2` |

For the two descriptor variants the firmware printed:

    DEFAULT CATCH!, exception-handler=fff00700
    at   %SRR0: 0000000000400000   %SRR1: 0000000000083002
    Open Firmware exception handler entered from non-OF code

So this firmware never reads a function descriptor: it jumps to `e_entry`.
It honours little-endian only for a 64-bit ELFv2 image. A 64-bit
little-endian ELFv2 ELF with one `PT_LOAD` and no `PT_NOTE`, on plain
ISO9660 with no partition table, is enough, which answers the epic's open
question 1: no 32-bit big-endian wrapper is needed.

The `<boot-script>boot &device;:\ppc\spike.elf</boot-script>` form works,
as on SLOF. The file name does matter. With the ELF stored as
`spike-le64v2.elf` or `spike-be32.elf` (Rock Ridge names; their ISO9660
names are truncated and have `_` for `-`) the firmware printed, for every
variant tried, with or without a CHRP `PowerPC` note in the ELF and with or
without a CHRP partition entry in the ISO:

    No OS image was detected by firmware.
    At least one disk in the bootlist was not found yet.
    Firmware is now retrying the entries in the bootlist.

The same ELFs stored as `spike.elf`, `be32.elf` or `le64.elf` loaded.
Whether length or the hyphen is what fails was not separated here. This is
not a difference from SLOF: checked afterwards on QEMU, SLOF loads
`spike.elf` and fails with `failed to load CHRP boot loader` for
`spike-le64v2.elf`, `spikelonger.elf` and `sp-ke.elf` alike. Neither
firmware uses the Rock Ridge name. `ipxe.elf`, the name `util/genfsimg`
uses, is found by both.

### Entry state (`le64v2`)

| Item | SLOF | PowerVM firmware |
|---|---|---|
| Load address | `0x400000` | `0x400000`; run-time and link-time address equal |
| `r3`, `r4` | `0`, `0` | `0`, `0` |
| `r5` | client interface, code | `0x00c3da70`, code (`b .+0x18` first), not a descriptor |
| `r6`, `r7` | argument string, `0` | `0`, `0` |
| `r12`, `r13`, `r2` | entry, entry, `0` | entry, `0`, `0` |
| `r1` | firmware stack | `0x08a9ffe0`, inside the real-mode area |
| MSR | `0x8000000000003001` | `0x8000000000003003`: SF, FP, ME, RI, LE; EE, IR, DR, HV clear |
| `.bss` | zero | 190 of 256 sampled bytes non-zero |

Client-interface calls worked with MSR[SF] set (`le64v2`) and clear (`sf0`,
`le32`, `be32`). After `sf0`'s calls the MSR was the entry MSR again, so
the firmware preserves `r30` in full. `exit` returns to the firmware
prompt (`EXIT called`).

### Real-mode area and `claim`

    SPIKE node memory /memory@0
    SPIKE   reg len=0x10 data=00000000000000000000000010000000
    SPIKE   available len=0x30 data=0000000000004000 00000000003fc000
                                   000000000040af40 00000000007f50c0
                                   00000000092a5000 0000000006d5b000

(The `available` value is split into its base and size pairs here.)
`/memory@0` is the only `memory` node and covers 256 MiB. The rest of the
partition's 4 GiB is described by `/ibm,dynamic-reconfiguration-memory`
(`ibm,dynamic-memory-v2`, `ibm,lmb-size`), not by `memory` nodes. Free are
`0x4000` to 4 MiB, the end of the image to 12 MiB, and about 146 MiB to
256 MiB; the firmware holds 12 MiB to 146 MiB.

| `claim` | Result |
|---|---|
| 1 MiB at 16 MiB (inside the firmware's range) | fails |
| 1 MiB at the top of the area (255 MiB) | succeeds; written and read back |
| 1 MiB at 256 MiB and at each higher address tried | fails |
| any 1 MiB, 4 KiB aligned | `0x4000` |
| any 256 MiB, any 1 GiB | fails |

iPXE's heap comes from the largest `available` range, so it has about
109 MiB here, less than one installer initrd. `/chosen` has
`ibm,client-architecture-support-reboot`; the call itself was not made
(#38).

### Raw frames on the virtual Ethernet node

The node is `/vdevice/l-lan@30000002` (`compatible` `IBM,l-lan`,
`device_type` `network`) and the `net` alias names it. `open` of the alias,
and then of the path, each produced

    BOOTP Parameters:
    [...]
    device              = /vdevice/l-lan@30000002
    MAC address         = <mac>
    loc-code            = <loc>
    BOOTP request retry attempt: 1
    [...]
    BOOTP request retry attempt: 4
            !BA01B015 !

and returned no instance, about two minutes after it was called (the
reference codes for the start of BOOTP and for its failure are 127 seconds
apart). `open` of the path followed by
`:speed=auto,duplex=auto,` and the dummy addresses GRUB uses returned an
instance at once. With it:

    SPIKE net frames seen before send = 0x25
    SPIKE net arp target learned from traffic = 0x1
    SPIKE net write(60) returned = 0x3c
    SPIKE net frames seen after send = 0x1
    SPIKE net arp reply received = 0x1
    SPIKE net reads that returned no frame = 0xca
    SPIKE net last no-frame read returned = 0x0

`read` returned 37 frames in two seconds without being asked for any, one
ARP probe was written, and the reply to it was read. A `read` with nothing
waiting returns `0` at once, as on SLOF (#39).

### RTAS

`/rtas` has `rtas-size` `0x1360000` (about 19 MiB; `0x82c` on SLOF) and a
`get-time-of-day` token of `3`. One attempt was made, on the first
partition, from the `be32` variant: `claim(any, rtas-size, 4 KiB)` returned
`0x092a5000`, the lowest address of the free range above the firmware, and
the probe then called `instantiate-rtas` with that base. (That ELF also
carried a CHRP `PowerPC` note, added while file naming was being ruled
out.) The console
printed nothing more from the probe, then

    Lost connection to the Managed System...

and the run ended. The first partition was not used again. The step was
not repeated and no other RTAS call was
made, so RTAS on PowerVM is unproven: instantiation, time of day, and a
second instantiation by a kernel. The cause is not established (#40).

Separately, and from the code rather than a run: `ieee1275_rtas.c` refuses
an `rtas-size` above `RTAS_MAX_SIZE` (1 MiB) before it claims or
instantiates anything. With 19 MiB here, iPXE's wall-clock, reboot and
power-off providers cannot work on this firmware whatever #40 finds, and
the RTAS image would take 19 MiB of the 109 MiB free range (#41).

### Processor version and DARN

`cpu-version` is `0x0f000005`, the architected ISA 3.0 compatibility
value, while the PVR read with `mfspr` is `0x004e0202`. `darn` (64-bit,
conditioned) returned a value in that mode in every variant that ran. A
mode older than ISA 3.0 was not tried (#32).

## iPXE ISO

Normal-mode activation, no key sent:

    iPXE initialising devices...
    iPXE 2.0.0+ (g83278) -- Open Source Network Boot Firmware -- https://ipxe.org
    Features: DNS HTTP HTTPS TFTP VLAN ELF Menu
    BOOTP Parameters:
    [...]
    BOOTP request retry attempt: 4
            !BA01B015 !
    Could not open net0: No such device (https://ipxe.org/2c80e013)
    Boot failed, returning to the menu
    Press any key
    ppc64le installer
      Fedora 44 Server / Rocky Linux 9 / iPXE shell

(The commit in the banner is a local one whose `src/` is that of
`8e9744fff`.) Result: the firmware reads the ISO that `util/genfsimg`
writes and loads `ipxe.elf`; console, timer and memory map initialisation
work; the network device is found but cannot be opened (#39), the script's
`ifopen` fails, and the menu is shown. Nothing was fetched.

## Firmware prompt

The partition's Open Firmware prompt is restricted: `boot`, `devalias`
and `dir` are refused, and only a set of macros is accepted.
`BOOT_FROM_DEVICE #cd/dvd <loc>` boots the optical device through its
`bootinfo.txt` and was used for the `be32` run; the other results above
are from normal-mode activations.

## Not proven

- RTAS in any form (#40), and an iPXE image that contains the RTAS
  providers (which the size limit would stop first, #41).
- Anything after `ibm,client-architecture-support`, including whether it
  restarts the partition (#38).
- A kernel hand-off, HTTPS, or any fetch (#14).
- That the SLOF network `open` accepts the arguments used here.
- Which property of a file name PowerVM firmware rejects (SLOF rejects
  both a long name and a hyphen).
- A processor compatibility mode older than ISA 3.0, POWER10 or later,
  another firmware level, or a partition with more than one network or
  optical device.
- Claims at or above 4 GiB (32-bit cells), as on SLOF.
