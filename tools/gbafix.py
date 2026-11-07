#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Makes a GBA ROM bootable on real hardware, as devkitPro's gbafix does:
#   - writes the 156-byte boot logo that the console's BIOS compares against,
#   - writes the fixed header byte 0x96 and clears the device type,
#   - writes the header complement checksum (byte 0xBD),
#   - pads the ROM with 0xFF to a power-of-two size (flash cartridges expect this).
# Usage: gbafix.py ROM LOGO, where LOGO is the file tools/fetch_logo.py downloads (the logo is not kept in the repo).
import sys

path = sys.argv[1]
LOGO = open(sys.argv[2], "rb").read()
data = bytearray(open(path, "rb").read())
if len(data) < 0xC0:
    raise SystemExit("ROM is smaller than its header")
if data[3] != 0xEA:
    raise SystemExit("ROM does not start with a branch instruction")
assert len(LOGO) == 156
data[0x04:0xA0] = LOGO
data[0xB2] = 0x96
data[0xB4] = 0x00
data[0xBD] = (-(sum(data[0xA0:0xBD]) + 0x19)) & 0xFF

size = 1 << (len(data) - 1).bit_length()
size = max(size, 0x100000)
data += bytes([0xFF]) * (size - len(data))
open(path, "wb").write(data)
print(f"{path}: logo and header written, checksum 0x{data[0xBD]:02x}, padded to {size // 1024} KiB")
