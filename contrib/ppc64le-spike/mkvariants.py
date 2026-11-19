#!/usr/bin/env python3
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
"""Derive ELF class/endianness variants of spike.elf.

Only the ELF container changes: the loaded segment is the same bytes
(its first 24 bytes are the function descriptor used by the variants
whose entry point is a descriptor).  Usage: mkvariants.py spike.elf DIR
"""
import struct
import sys

CODE = 0x400018         # _start (spike.lds asserts this)
BASE = 0x400000         # segment load address, also the descriptor


def segment(elf):
    phoff, = struct.unpack_from("<Q", elf, 32)
    off, vaddr, paddr, filesz, memsz = struct.unpack_from(
        "<QQQQQ", elf, phoff + 8)
    assert vaddr == paddr == BASE
    return bytearray(elf[off:off + filesz]), memsz


def with_descriptor(seg, endian):
    seg[0:16] = struct.pack(endian + "QQ", CODE, 0)
    return seg


def elf64(seg, memsz, endian, entry, flags):
    ident = b"\x7fELF\x02" + (b"\x01" if endian == "<" else b"\x02")
    ident += b"\x01" + bytes(9)
    ehdr = ident + struct.pack(endian + "HHIQQQIHHHHHH", 2, 21, 1, entry,
                               64, 0, flags, 64, 56, 1, 64, 0, 0)
    phdr = struct.pack(endian + "IIQQQQQQ", 1, 7, 0x1000, BASE, BASE,
                       len(seg), memsz, 0x1000)
    return (ehdr + phdr).ljust(0x1000, b"\0") + bytes(seg)


def elf32(seg, memsz, endian, entry):
    ident = b"\x7fELF\x01" + (b"\x01" if endian == "<" else b"\x02")
    ident += b"\x01" + bytes(9)
    ehdr = ident + struct.pack(endian + "HHIIIIIHHHHHH", 2, 20, 1, entry,
                               52, 0, 0, 52, 32, 1, 40, 0, 0)
    phdr = struct.pack(endian + "IIIIIIII", 1, 0x1000, BASE, BASE,
                       len(seg), memsz, 7, 0x1000)
    return (ehdr + phdr).ljust(0x1000, b"\0") + bytes(seg)


def main():
    elf = open(sys.argv[1], "rb").read()
    out = sys.argv[2]
    seg, memsz = segment(elf)
    variants = {
        "le64v2": elf,
        "le64v1": elf64(with_descriptor(bytearray(seg), "<"), memsz,
                        "<", BASE, 1),
        "be64": elf64(with_descriptor(bytearray(seg), ">"), memsz,
                      ">", BASE, 0),
        "le32": elf32(seg, memsz, "<", CODE),
        "be32": elf32(seg, memsz, ">", CODE),
    }
    for name, data in variants.items():
        with open("%s/spike-%s.elf" % (out, name), "wb") as f:
            f.write(data)


main()
