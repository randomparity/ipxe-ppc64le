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

# Set RTAS_CALLS=0 to build a probe that reads the RTAS properties but
# neither instantiates nor calls RTAS
RTAS_CALLS ?= 1
CFLAGS	+= -DRTAS_CALLS=$(RTAS_CALLS)

all : spike.elf spike-sf0.elf

# Discard every object when the compiler or its flags change, so that
# RTAS_CALLS=0 cannot silently reuse objects built with RTAS calls
.flags : FORCE
	@echo '$(CC) $(CFLAGS)' | cmp -s - $@ || \
		{ rm -f *.o spike*.elf ; echo '$(CC) $(CFLAGS)' > $@ ; }

FORCE :

spike.elf : entry.o probe.o spike.lds
	$(CC) $(LDFLAGS) -o $@ entry.o probe.o

# The same probe, calling the firmware in 32-bit mode
spike-sf0.elf : entry-sf0.o probe.o spike.lds
	$(CC) $(LDFLAGS) -o $@ entry-sf0.o probe.o

entry-sf0.o : entry.S .flags
	$(CC) $(CFLAGS) -DCALL_SF0 -Wa,-mregnames -c -o $@ $<

%.o : %.S .flags
	$(CC) $(CFLAGS) -Wa,-mregnames -c -o $@ $<

%.o : %.c .flags
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f *.o spike*.elf .flags
