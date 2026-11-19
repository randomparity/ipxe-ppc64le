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

# Open Firmware spike: build a standalone ppc64le ELF (no iPXE core)
#
# Usage: make CROSS=powerpc64le-linux-gnu-

CROSS	?= powerpc64le-linux-gnu-
CC	:= $(CROSS)gcc
OBJCOPY	:= $(CROSS)objcopy
CFLAGS	:= -Os -Wall -Werror -ffreestanding -fno-pic -fno-pie -mcpu=power8 \
	   -mno-altivec -mno-vsx -fno-stack-protector -mabi=elfv2 \
	   -fno-asynchronous-unwind-tables
LDFLAGS	:= -nostdlib -static -no-pie -Wl,-T,spike.lds -Wl,--no-warn-rwx-segments \
	   -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack -Wl,--build-id=none

all : spike.elf

spike.elf : entry.o probe.o spike.lds
	$(CC) $(LDFLAGS) -o $@ entry.o probe.o

%.o : %.S
	$(CC) $(CFLAGS) -Wa,-mregnames -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f *.o spike*.elf
