# ppc64le end-to-end run record: QEMU pseries (SLOF)

One run of `installer-menu.ipxe` for each distribution. Every result below
was observed in that run. Console text is excerpted from the full capture,
with ANSI escapes removed, progress bars shortened and the elisions marked
`[...]`; nothing else was edited. Nothing in the capture needed redaction:
the only addresses are the QEMU user-networking defaults.

This file is the format for later runs (for example the PowerVM run, #14):
the header names every input, each distribution has an excerpt and a result
line, and "Not proven" lists what the run does not show.

## Inputs

| Item | Value |
|---|---|
| iPXE | `1887facad` (ppc64le-support), `bin-ppc64le-ieee1275/ipxe.iso` |
| Build | `make -C src bin-ppc64le-ieee1275/ipxe.iso CROSS=powerpc64le-linux-gnu- EMBED=<script>`, GCC 14.2.0 cross compiler in a container |
| Script | `installer-menu.ipxe`, with one edit (see "Deviations") |
| Emulator | QEMU 10.2.2 `-M pseries -cpu power9`, TCG, SLOF release 20220719 |
| Command | `qemu-system-ppc64 -M pseries -cpu power9 -smp 4 -m 4G -nographic -cdrom ipxe.iso -boot d -nic user,model=spapr-vlan` |
| Input | one key at the menu (`f` or `r`), sent through the console |
| Fedora | `https://dl.fedoraproject.org/pub/fedora-secondary/releases/44/Server/ppc64le/os`, `ppc/ppc64/vmlinuz` (66 MB), `initrd.img` (225 MB) |
| Rocky | `https://mirrors.xtom.com/rocky/9/BaseOS/ppc64le/os`, same layout, `vmlinuz` (47 MB), `initrd.img` (213 MB) |

Both kernels were checked with `file`: uncompressed 64-bit little-endian
PowerPC ELF executables, which `kernel` accepts.

## Deviations from the checked-in script

- `net0/dns` was `1.1.1.1` instead of `10.0.2.3`. QEMU's DNS proxy at
  `10.0.2.3` forwards to the host's resolver; on the host used, that is a
  loopback stub that the proxy could not use, and every name lookup in the
  guest timed out. Fetching by address and with `1.1.1.1` worked. This is a
  property of the host, not of iPXE.
- Rocky was first tried at `download.rockylinux.org`, which fails (see
  "Defect found" below), and is served from `mirrors.xtom.com` instead.

## Fedora 44 Server

Key `f` at the menu. From the keypress to the installer prompt: 416 s.

    ppc64le installer
      Fedora 44 Server / Rocky Linux 9 / iPXE shell
    https://dl.fedoraproject.org/.../ppc/ppc64/vmlinuz.... [OCSP iPXE cross-signing CA G2] ... ok
    https://dl.fedoraproject.org/.../ppc/ppc64/initrd.img... 0% ... 97% ok
    Preparing to boot Linux version 6.19.10-300.fc44.ppc64le [...]
    command line: console=hvc0 inst.text ip=10.0.2.15::10.0.2.2:255.255.255.0:installer::none nameserver=1.1.1.1 inst.repo=https://dl.fedoraproject.org/.../os
    [...]
    Quiescing Open Firmware ...
    Booting Linux via __start() @ 0x0000000000900000 ...
    [    0.000000] Found initrd at 0xc000000087e10000:0xc00000009545c0a4
    [    0.000000] Hardware name: IBM pSeries (emulated by qemu) POWER9 (architected) 0x4e1202 0xf000005 of:SLOF,HEAD pSeries
    [...]
    ibmveth 71000002 env2: renamed from eth0
    [...]
    Starting installer, one moment...
    anaconda 44.30-2.fc44 for Fedora 44 started.
    [...]
    Text mode provides a limited set of installation options. [...]
    1) Use graphical mode via Remote Desktop Protocol
    2) Use text mode
    Please make a selection from the above ['c' to continue, 'q' to quit, 'r' to refresh]:

Result: pass. The kernel and initrd came over HTTPS, the installer fetched
its own stage 2 from `inst.repo` over the static address, and anaconda
waits for input.

## Rocky Linux 9

Key `r` at the menu. From the keypress to the installer prompt: 459 s.

    https://mirrors.xtom.com/.../ppc/ppc64/vmlinuz.... [XCRT GTS Root R1] ... ok
    https://mirrors.xtom.com/.../ppc/ppc64/initrd.img... 1% ... 99% ok
    Preparing to boot Linux version 5.14.0-741.22.1.el9_9.ppc64le [...]
    command line: console=hvc0 inst.text ip=10.0.2.15::10.0.2.2:255.255.255.0:installer::none nameserver=1.1.1.1 inst.repo=https://mirrors.xtom.com/rocky/9/BaseOS/ppc64le/os
    [...]
    Booting Linux via __start() @ 0x0000000000900000 ...
    [...]
    [   49.829149] ibmveth 71000002 env2: renamed from eth0
    [...]
    Starting installer, one moment...
    anaconda 34.25.7.19-1.el9.rocky.0.6 for Rocky Linux 9.9 started.
    [...]
    Text mode provides a limited set of installation options. [...]
    1) Start VNC
    2) Use text mode
    Please make a selection from the above ['c' to continue, 'q' to quit, 'r' to refresh]:

Result: pass, as for Fedora.

## Defect found: HTTPS to `download.rockylinux.org` fails

`imgfetch https://download.rockylinux.org/pub/rocky/9/BaseOS/ppc64le/os/media.repo`
fails with `Error 0x420c6020` (`EPIPE` from the HTTP core), and a repeat
within the same session with "Operation not permitted" (`TLS` fatal alert).
Observed facts:

- The same fetch fails with the same error from an x86_64 BIOS build of the
  same tree (`0x420c6001`, with HTTPS enabled by a local override), so it is
  not specific to ppc64le.
- Certificate validation succeeds. With `DEBUG=tls,httpcore:2` the server
  sends a NewSessionTicket and then a close_notify alert before iPXE has sent
  the HTTP request. For a server that works, the request is sent immediately
  after the client Finished.
- The same fetch works against four other Rocky mirrors and against
  `dl.fedoraproject.org`. The failing names answer with `Via: varnish` headers, so they appear to be CDN-fronted.
- Repeating the fetch after the certificate chain is cached still fails, so
  slow validation is not the cause.

The cause is not established. Because it is not in the ppc64le code, it was
not fixed or worked around here.

## Not proven

- PowerVM, POWER8, or a CPU other than `-cpu power9`; the default `pseries`
  CPU was not run.
- Guest memory below 4 GiB, or more than one NIC.
- Anything past the first installer prompt: no install was performed.
- The fetches were not compared with the published checksums; HTTPS and the
  built-in trusted roots were the only authenticity check.
