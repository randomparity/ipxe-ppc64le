#!/bin/sh
# Copyright (C) 2026 David Christensen
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or any later version.
#
# You can also choose to distribute this program under the terms of
# the Unmodified Binary Distribution Licence (as given in the file
# COPYING.UBDL), provided that you have satisfied its requirements.
#
# Boot spike ELFs from an ISO on QEMU pseries (SLOF) and capture the console.
#
# Usage: ./run.sh                    build the ELF variants and run them all
#        ./run.sh ELF [MEM [OUTDIR]] run one ELF
#
# Set BOOTINFO=file to use that /ppc/bootinfo.txt (BOOTINFO=/dev/null: empty).
# Requires qemu-system-ppc64, xorriso, tcpdump (optional) and python3.
# Per run, OUTDIR holds console.log (the evidence), net.pcap and qemu.out.
# Exit status is non-zero unless every run printed "SPIKE done".

set -u
QEMU=${QEMU:-qemu-system-ppc64}
LIMIT=${LIMIT:-60}
HERE=$(cd "$(dirname "$0")" && pwd)

run_one () { # elf mem outdir
	elf=$1 mem=$2 out=$3
	mkdir -p "$out/iso/ppc"
	cp "$elf" "$out/iso/ppc/spike.elf"
	rm -f "$out/iso/ppc/bootinfo.txt"
	if [ -n "${BOOTINFO:-}" ]; then
		cp "$BOOTINFO" "$out/iso/ppc/bootinfo.txt"
	else
		cat > "$out/iso/ppc/bootinfo.txt" <<'BOOTINFO'
<chrp-boot>
<description>ppc64le spike</description>
<os-name>spike</os-name>
<boot-script>boot &device;:\ppc\spike.elf</boot-script>
<bootfile>/ppc/spike.elf</bootfile>
</chrp-boot>
BOOTINFO
	fi
	xorriso -as mkisofs -quiet -R -o "$out/spike.iso" "$out/iso" || return 1
	: > "$out/console.log"
	"$QEMU" -machine pseries -m "$mem" -nographic -monitor none \
		-serial "file:$out/console.log" -no-reboot \
		-cdrom "$out/spike.iso" -boot d \
		-netdev user,id=n0 -device spapr-vlan,netdev=n0 \
		-object filter-dump,id=f0,netdev=n0,file="$out/net.pcap" \
		< /dev/null > "$out/qemu.out" 2>&1 &
	pid=$!
	i=0
	while [ $i -lt "$LIMIT" ] && kill -0 $pid 2> /dev/null; do
		grep -aq "SPIKE done" "$out/console.log" && break
		sleep 1
		i=$((i + 1))
	done
	kill $pid 2> /dev/null
	wait $pid 2> /dev/null
	grep -a 'SPIKE' "$out/console.log" | tr -d '\r' > "$out/spike.txt"
	command -v tcpdump > /dev/null &&
		tcpdump -nn -e -r "$out/net.pcap" > "$out/net.txt" 2>&1
	grep -aq 'SPIKE done' "$out/console.log"
}

if [ $# -ge 1 ]; then
	run_one "$1" "${2:-1G}" "${3:-out}"
	rc=$?
	cat "${3:-out}/spike.txt"
	exit $rc
fi

rc=0
mkdir -p out/variants
python3 -I "$HERE/mkvariants.py" spike.elf out/variants || exit 1
for v in le64v2 le64v1 be64 le32 be32; do
	run_one "out/variants/spike-$v.elf" 1G "out/$v-1G" ||
		{ echo "FAILED: $v"; rc=1; }
done
for mem in 2G 4G; do
	run_one out/variants/spike-le64v2.elf $mem "out/le64v2-$mem" ||
		{ echo "FAILED: le64v2 $mem"; rc=1; }
done
exit $rc
