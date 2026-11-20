# ppc64le firmware spike: QEMU pseries / SLOF

A standalone, hand-written ppc64le ELF with no iPXE core. It exists to
produce this record and the proven entry stub and call thunk that the
platform skeleton reuses. It is not part of the iPXE build.

Every fact below is either an observed result of a run reproducible with
the commands in "Build and run", or a citation of source at a pinned
version. Anything not observed is listed under "Not proven".

## Files

| File | Purpose |
|---|---|
| `entry.S` | Entry stub (endianness polyglot), state capture, `be_call` thunk |
| `probe.c` | Probe body: prints `SPIKE ...` result lines |
| `spike.lds` | Link script: one RWX `PT_LOAD` at `0x400000`, entry after a 24-byte descriptor slot |
| `mkvariants.py` | Derives ELF class/endianness variants from `spike.elf` (container only) |
| `run.sh` | Builds the ISO, boots QEMU, captures the console and a packet capture; `iso` mode only writes the ISOs |
| `Makefile` | `make CROSS=<toolchain prefix>`: `spike.elf`, and `spike-sf0.elf`, which calls the firmware in 32-bit mode; `RTAS_CALLS=0` builds a probe that makes no RTAS call |

## Build and run

Needs a ppc64le cross toolchain (`CROSS` is its prefix, for example
`powerpc64le-linux-gnu-`), plus `qemu-system-ppc64`, `xorriso`, `python3`
and optionally `tcpdump`.

    cd contrib/ppc64le-spike
    make CROSS=powerpc64le-linux-gnu-
    ./run.sh                      # all ELF variants at 1G, plus 2G and 4G
    ./run.sh iso [OUTDIR]         # one ISO per ELF, without starting QEMU

`./run.sh ELF [MEM [OUTDIR]]` runs one ELF. The `iso` mode is for firmware
other than QEMU's: it writes `spike-<variant>.iso` for the five variants
and for `spike-sf0.elf` (default `OUTDIR`: `out/iso`). In each ISO the
ELF is `/ppc/spike.elf`: PowerVM firmware did not find files with longer,
hyphenated Rock Ridge names, so the name is kept a plain ISO9660 one. Each run leaves
`OUTDIR/console.log` (the raw evidence), `spike.txt` (the `SPIKE` lines)
and `net.pcap`, plus `net.txt` if `tcpdump` is installed. `run.sh` exits non-zero unless every run printed
`SPIKE done`.

Observed with GCC 14.2.0 and binutils 2.44 (`powerpc64le-linux-gnu`),
QEMU 10.2.2 `-machine pseries` (alias of `pseries-10.2`, default CPU, PVR
`0x801200`), and the SLOF bundled with that QEMU package, "release
20220719" (upstream tag `qemu-slof-20220719`, commit
`6b6c16b4b40763507cf1f518096f3c3883c5cf2d`). On an x86_64 host the build
takes under a second inside a container; `./run.sh` took about 50 seconds
for its seven runs (roughly 7 seconds each, each ending at `SPIKE done`). The build is the
`make` line above run with the cross toolchain on its `PATH`; any
container or host install that provides it will do.

The SLOF line citations below are against the upstream tag; that the
distribution's package build matches that tag is assumed from its
"release 20220719" banner, not checked.

The probe was later made portable to PowerVM firmware (see "Portable
probe" below and `../ppc64le/powervm-probe-record.md`). The results in
sections 1 to 6 were reproduced with it, on QEMU 10.0.13 (Debian 13
package, default CPU, PVR `0x801200`) and that package's SLOF, "release
20230918", with the same compiler and binutils: eight runs (the five
variants at 1G, `le64v2` at 2G and 4G, `spike-sf0.elf` at 1G), each ending
at `SPIKE done`, in about a minute in all. Where a value below differs in
that re-run, both are given.

## Results

### 1. ELF class and endianness accepted, booting from an ISO

SLOF loaded and entered every variant from an ISO9660 image via
`/ppc/bootinfo.txt` (the `Trying to load ... Successfully loaded` line in
each `console.log`, followed by the probe output).

| Variant | ELF class / data | `e_flags` | `e_entry` | Result |
|---|---|---|---|---|
| `le64v2` | 64, little | 2 (ELFv2) | code | accepted; entered little-endian, MSR `0x8000000000003001` |
| `le64v1` | 64, little | 1 (ELFv1) | descriptor | accepted; entered little-endian, MSR `0x8000000000003001` |
| `be64` | 64, big | 0 | descriptor | accepted; entered big-endian, MSR `0x8000000000003000` |
| `le32` | 32, little | 0 | code | accepted; entered little-endian, MSR `0x0000000000003001` (SF clear) |
| `be32` | 32, big | 0 | code | accepted; entered big-endian, MSR `0x0000000000003000` (SF clear) |

All five carry the same segment bytes; only the ELF container differs.
SLOF picks the entry convention from the ELF header
(`lib/libelf/elf.c:94-109`, `slof/fs/boot.fs:182-190` at the tag above):
ELFv2 little-endian jumps to `e_entry` directly with `r12 = r13 = entry`;
ELFv1 and big-endian 64-bit read a function descriptor at `e_entry`.
`le64v2` is the one to use: SLOF enters it in the right endianness.

SLOF loads each `PT_LOAD` to `p_paddr` (`lib/libelf/elf64.c:220`) and
warns if `p_paddr != p_vaddr` (`elf64.c:254`).

`bootinfo.txt` forms tried, each on `le64v2`:

| `/ppc/bootinfo.txt` | Result |
|---|---|
| `<boot-script>boot &device;:\ppc\spike.elf</boot-script>` and `<bootfile>` | loaded |
| `<boot-script>` only (backslash path) | loaded |
| `<bootfile>/ppc/spike.elf</bootfile>` only | not loaded; SLOF falls through to the network |
| `<boot-script>boot &device;:/ppc/spike.elf</boot-script>` (forward slashes) | not loaded: `failed to load CHRP boot loader.` |
| empty file | not loaded |

SLOF only reads `<boot-script>` and takes the text after `&device;:`
(`slof/fs/packages/disk-label.fs:545-566`), so the path must use
backslashes. Use `BOOTINFO=<file> ./run.sh ELF` to repeat.

### 2. MSR, registers and load address at entry (`le64v2`, `-m 1G`)

| Item | Observed |
|---|---|
| Load address | `0x400000` (= `p_paddr`); run-time and link-time address of one probe point equal (`0x400098`) |
| `r3`, `r4` | `0`, `0` (initrd start and length; none supplied) |
| `r5` | `0x3daf04c4`, the client-interface entry. It points at code, not a descriptor: the first dword is `mflr r4; bl ...` |
| `r6`, `r7` | argument string pointer (`0x3e6886b0`; `0x3e689588` in the re-run), length `0` |
| `r12`, `r13` | both the entry address; `r2 = 0` |
| `r1` | `0x3db7b400` (`0x3db7c400` in the re-run), a firmware-provided stack, inside RAM |
| MSR | `0x8000000000003001`: SF=1, FP=1, ME=1, LE=1; EE=0, IR=0, DR=0, HV=0 (real mode, interrupts off) |
| PVR | `0x801200` (QEMU default CPU) |

With more RAM `r1`, `r5` and `r6` move up (`0x7d...` at 2G and 4G); only
their being below 4 GiB is relied on. For the 32-bit variants SF is clear.

### 3. Real-mode area and `claim`

| `-m` | `/memory@0` `reg` (base, size) | `claim` of 1 MiB at a fixed address |
|---|---|---|
| 1G | `0`, `0x40000000` | succeeds at every tested address below 1 GiB (`0x1000000` to `0x3ff00000`); fails at `0x40000000` and above (past RAM) |
| 2G | `0`, `0x80000000` | succeeds up to `0x7ff00000`; fails at `0xbff00000` and `0xfff00000` (past RAM) |
| 4G | `0`, `0x100000000` | succeeds at `0x40000000`, `0x7ff00000`, `0xbff00000`, `0xfff00000` |

Every successful claim was written and read back with the MMU off
(`real-mode store/load round trip ok`). No RMA property is published by
SLOF; the first memory node covers all RAM. QEMU sets the RMA size to
the lesser of RAM, the first NUMA node and 1 TiB (`hw/ppc/spapr.c:2798-2821`
at QEMU `v10.2.2`) and publishes it as the first memory node
(`spapr.c:653-657`). Linux on a logical partition takes the size of the
`base == 0` memory node as its real-mode limit and caps it at `0x30000000`
(`arch/powerpc/kernel/prom_init.c:1716-1717` and `1756-1759`, tag `v7.0`).

So on SLOF `claim` does return memory above any smaller assumed limit, up
to the 4 GiB boundary. Client-interface cells are 32 bits
(`slof/fs/client.fs:30-33` reads them with `l@`), so addresses of 4 GiB
and above cannot be passed or returned: this spike could not test them.
`claim(any, ...)` with a nonzero alignment is a best-fit search of the free
list (`slof/fs/claim.fs:293-328`), so the result is neither lowest nor
highest: 1 MiB came back at `0x3fe04000` at 1G and `0x4000` at 4G.

### 4. Raw Ethernet frame through the network node (`spapr-vlan`)

`finddevice("net")` resolves the alias to `/vdevice/l-lan@71000003`;
`open("net")` returned an instance. One 60-byte ARP request (broadcast,
sender = the node's `local-mac-address`, QEMU's default MAC for the first
NIC) was sent with `write` (returned 60). Polling `read` returned one
64-byte frame, ethertype `0x0806`, ARP opcode 2, from the user-mode
network gateway's MAC (an emulator default). The packet capture agrees
(as re-run: the request is now an ARP probe, with sender address
`0.0.0.0`):

    52:54:00:12:34:56 > ff:ff:ff:ff:ff:ff, ARP, Request who-has 10.0.2.2 tell 0.0.0.0
    52:55:0a:00:02:02 > 52:54:00:12:34:56, ARP, Reply 10.0.2.2 is-at 52:55:0a:00:02:02

(Emulator-default MACs and user-mode network addresses.) Every `read` made
with nothing waiting returned `0` at once (197 of them in the six seconds
the probe listens); so `read` does not block and callers poll. This matches `veth_receive`, which returns 0
when no frame is queued (`lib/libveth/veth.c:167-203`, tag above).

### 5. RTAS

`/rtas` has `rtas-size = 0x82c` and `get-time-of-day` token `0x2001`.
`call-method "instantiate-rtas"` on the opened `/rtas` instance, with a
`claim`ed page-aligned buffer as the base, returned status 0 and an entry
equal to the base. `get-time-of-day` through that entry returned status 0
and the host's UTC date and time.

RTAS can be instantiated more than once: a second instance at a different
base worked, the first kept working afterwards, and instantiating again at
the first base also returned status 0. SLOF's `instantiate-rtas` simply
copies the RTAS image to the supplied address
(`board-qemu/slof/rtas.fs:150-152`). RTAS is called with `r3` = argument
block, `r4` = base, in big-endian mode, through the same `be_call` thunk.

### 6. Call thunk contract (`be_call`)

The client interface and RTAS are big-endian; the stub runs little-endian.
`be_call(entry, a0, a1, a2)` therefore: saves `r2`, `r13`-`r31`; computes
the MSR with LE cleared (SF left as it is); points LR at a three-instruction
big-endian return stub (`mtsrr0 r31; mtsrr1 r30; rfid`); and `rfid`s to
`entry`. On return the stub restores the little-endian MSR and the thunk
reloads its registers. Observed against SLOF:

- Arguments are an array of 32-bit big-endian cells (service string
  pointer, `nargs`, `nret`, then arguments and return slots); every
  pointer passed must be below 4 GiB.
- `r3` = argument array, nothing else needed; `r2` need not be set up.
  SLOF's entry swaps the whole register file and MSR with its own saved
  copy (`slof/entry.S:108-143`, `157-162`), so the callee preserves the
  caller's registers and returns in the MSR it was called with.
- Only LE is cleared for a call, so the 64-bit variants called with SF=1
  and the `le32`/`be32` variants (entered with SF clear) called with
  SF=0, the convention Linux uses (section 7). Both worked on SLOF: the
  client interface, `claim`, the network node and RTAS all returned the
  same results in both modes apart from the clock reading.
- The client-interface `exit` service returns control to SLOF, which goes
  on to the next boot device; the spike ends with `exit`.
- Calling the client interface without the thunk (little-endian) was not
  tried: SLOF's code is big-endian and cannot be decoded in LE mode.

The endianness polyglot at `_start` is the same mechanism as the kernel's
`FIXUP_ENDIAN` (`arch/powerpc/include/asm/ppc_asm.h:836-849`, tag `v7.0`);
its trampoline words were assembled independently and match, apart from
an added `li r12,1` that marks a big-endian entry. It was
exercised through the `be64` and `be32` variants (`entered_big_endian = 1`).

### Portable probe

What the probe does so that one binary runs on SLOF and on PowerVM
firmware, and what that showed on SLOF:

- It walks the device tree and prints each node whose `device_type` is
  `memory`, `network` or `cpu`, instead of reading `/memory@0` alone. On
  SLOF: `/cpus/PowerPC,POWER10@0` (`cpu-version` `0x801200`,
  `timebase-frequency` 512000000), `/vdevice/l-lan@71000003` (`compatible`
  `IBM,l-lan`) and `/memory@0`, whose `available` property lists the
  unclaimed ranges. `/ibm,dynamic-reconfiguration-memory` is absent.
- The real-mode area is taken as the size of the memory node at address 0,
  and two more 1 MiB claims are made relative to it: at its top (succeeds)
  and just above it (fails on SLOF with 1G or 2G, where that is the end of RAM;
  skipped at 4G, which cells cannot address). Claimed
  memory is written only inside that area.
- The network node is opened as the `net` alias, then by its path, then
  by its path with the arguments GRUB appends (dummy addresses that stop
  IBM partition firmware from running BOOTP); the first that works is
  used. On SLOF the alias opens.
- The ARP request is a probe (sender address `0.0.0.0`). The probe first
  listens for two seconds and, if it sees an ARP packet from another host,
  asks for that host instead of the QEMU gateway, so no address has to be
  configured. No frame arrives unasked on the user-mode network, so on
  QEMU the gateway is asked. MAC and IP addresses, `bootargs` and
  `bootpath` are not printed (lengths only).
- The MSR is printed after the first firmware calls and again at the end.
  On SLOF it is the entry MSR with LE set in every variant, and with
  `spike-sf0.elf` (calls made with SF and LE clear) SF is set again after
  each call: SLOF preserves `r30`, from which the return stub reloads it.
- A failed RTAS `claim` or `instantiate-rtas` is printed and the calls
  that depend on it are skipped. None fails on SLOF. Built with
  `RTAS_CALLS=0` the probe prints `rtas-size` and the token and neither
  opens, instantiates nor calls RTAS.
- It counts the non-zero bytes in 256 bytes of its own `.bss` before it
  uses any, then clears the variables it relies on. SLOF zeroes `.bss`
  (count 0); the stub does not, so the count shows what a firmware does.
- Last, it executes `darn` (64-bit, conditioned). With QEMU's default CPU
  it returns a value. On a processor or in a mode without the instruction
  it traps and the run ends there, which is why it is last.

### 7. Linux kernel Open Firmware entry contract (tag `v7.0`, commit `028ef9c96e96197026887c0f092424679298aae8`)

All paths under `arch/powerpc/kernel/` unless stated.

| Item | Contract | Citation |
|---|---|---|
| State | MMU off, firmware in real mode; primary CPU enters at `__start` (offset 0 of the image) | `head_64.S:56-60`, `93` |
| Endianness | Either: `FIXUP_ENDIAN` at `__start` switches a big-endian entry to little-endian | `head_64.S:96`, `ppc_asm.h:836-849` |
| Width | `enable_64b_mode` first, so a 32-bit-mode entry is fine | `head_64.S:528-530` |
| Registers | `r5 != 0` means Open Firmware entry; `r3`...`r7` carry "legacy" content; `r5` is the client-interface entry (code address) | `head_64.S:519-543`, `prom_init.c:2222` (`prom_entry = pp`) |
| Initrd | `r3` = start, `r4` = length (ignored if either is 0 or `r4 == 0xdeadbeef`); the kernel then writes `linux,initrd-start`/`-end` to `/chosen` itself | `prom_init.c:3155-3170`, call at `3288` |
| Command line | Read from `/chosen` `bootargs` via the client interface; `r6`/`r7` are not used for it | `prom_init.c:794` |
| Callouts | Kernel calls the client interface in 32-bit big-endian mode (clears SF and LE in the MSR), enters at the `r5` address with `r3` = argument block, and tolerates a return in either endianness | `prom_entry_64.S:51-65` |
| Memory | Allocates below `min(0x30000000, first memory node size)` with `claim`; instantiates RTAS with `claim` + `call-method instantiate-rtas` | `prom_init.c:1716-1759`, `1837-1900` |
| Requires | `CONFIG_PPC_OF_BOOT_TRAMPOLINE=y` (the Ubuntu config below has it) | `head_64.S:578-619` |

SLOF itself passes `r3`/`r4` from `/chosen` `linux,initrd-start`/`-end`
(`slof/fs/client.fs:49-50`, tag above), which is why this spike saw `0`/`0`.

### 8. Program headers of a real distribution `vmlinux`

Read from `/boot/vmlinux-7.0.0-27-generic` of package
`linux-image-7.0.0-27-generic` version `7.0.0-27.27` (Ubuntu 26.04 LTS
ppc64el cloud image), file size 64563000 bytes, SHA-256
`c69a0e233ad867409cc75a0b7e329e2e1861897b2f9c33bdc0666cfab4304ee6`,
extracted read-only (`virt-copy-out` from the unmodified base image) and
inspected with `readelf -hlSW`:

    Class ELF64, little endian, Type EXEC, Machine PowerPC64, Flags 0x2 (abiv2)
    Entry point 0xc000000000000000
    LOAD  off 0x010000  vaddr 0xc000000000000000  paddr 0x0000000000000000
          filesz 0x3d81888  memsz 0x3f56b78  flags RWE  align 0x10000
    NOTE  off 0x22f2320 vaddr 0xc0000000022e2320  paddr 0x00000000022e2320
          filesz 0xdc  memsz 0xdc  align 0x4

Two program headers; no `PT_INTERP`, no dynamic section (303235 `.rela.dyn`
entries are applied by the kernel's own relocation code,
`CONFIG_RELOCATABLE=y`, `CONFIG_PHYSICAL_START=0x0`). Other config:
`CONFIG_CPU_LITTLE_ENDIAN=y`, 64 KiB pages, `CONFIG_PPC_OF_BOOT_TRAMPOLINE=y`,
`CONFIG_BLK_DEV_INITRD=y`.

Consequences for iPXE's ELF loader (`src/image/elf.c:109-140`): the single
segment has `p_paddr = 0`, so the loader's fall back to `p_vaddr` makes the
destination `0xc000000000000000`, not the physical address 0 the kernel
expects (a `p_paddr` of 0 is not what is rejected here). The entry
`0xc000000000000000` is a virtual address. SLOF (`elf64.c:220`) uses
`p_paddr`, so a kernel loaded by SLOF lands at 0.

## Implications for later issues

- #4 (call thunk, console): lift `be_call` and the three-word return
  stub. The thunk must own the endianness switch and the 32-bit cell
  packing; pointers handed to the client interface must be below 4 GiB.
- #6 (memory map): `/memory@0` `reg` is the usable real-mode range on
  SLOF; `claim` is best-fit; nothing at or above 4 GiB is reachable via the
  client interface.
- #10 (image and `bootinfo.txt`): ship a little-endian 64-bit ELFv2 ELF,
  `e_entry` the code address, and a `<boot-script>boot &device;:\path` line
  with backslashes; `<bootfile>` alone does not boot.
- #11 (kernel hand-off): pass `r3`/`r4` = initrd, `r5` = a client-interface
  entry, set `/chosen` `bootargs`; load the kernel at physical 0 (see 8).
  Observed in the iPXE implementation (QEMU 10.2.2 `-M pseries`, SLOF
  20220719, Fedora 44 ppc64le kernel 6.19.10 and its initramfs, `-m 512M` to
  `-m 4G`): the entry contract in 7 is sufficient as read, with two
  corrections. The kernel need not be loaded at 0: it is relocatable and
  copies itself there, so iPXE loads it in low memory it claims above its own
  image. And that memory must be below `alloc_top` (`0x30000000`, the RMA
  limit the kernel prints): loading it at the start of a heap claim that
  began above 2 GiB made `prom_init` fail with "Could not allocate memory for
  RTAS". The kernel also fails to claim its own memory unless iPXE first
  releases its claim on the rest of RAM.
- #13 (PowerVM): done; `../ppc64le/powervm-probe-record.md` lists what
  differs on PowerVM partition firmware.

## Not proven

- Anything on non-SLOF firmware other than what
  `../ppc64le/powervm-probe-record.md` records for PowerVM.
- `claim` of memory at or above 4 GiB (32-bit cells), and any real RMA
  limit enforcement (QEMU makes the RMA all of RAM).
- That SLOF can itself boot the distribution `vmlinux` from an ISO; the
  program headers were only read (iPXE boots it, see Implications).
- Spinning in a `b .` loop after the last probe, instead of calling
  `exit`, was seen once to end in a SLOF `( 700 ) Program Exception` about a
  second later, with a temporary build that no longer exists; it is not
  reproducible from this tree and its cause is unknown.
