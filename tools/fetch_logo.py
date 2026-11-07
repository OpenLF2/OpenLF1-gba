#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Downloads the 156-byte boot logo a GBA ROM header needs (it is not kept in this repository).

    fetch_logo.py [OUT]        writes OUT (default build/boot_logo.bin)

The bytes are read from devkitPro's gbafix.c; web/config.json names the same URL for the browser build."""
import os
import re
import sys
import urllib.request

URL = "https://raw.githubusercontent.com/devkitPro/gba-tools/master/src/gbafix.c"


def parse(text):
    """The logo array that follows the '// logo' comment in gbafix.c."""
    body = re.search(r"// logo\s*\n\s*\{(.*?)\}", text, re.S).group(1)
    logo = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body))
    if len(logo) != 156 or logo[:4] != bytes.fromhex("24ffae51"):
        raise ValueError("unexpected boot logo data")
    return logo


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "boot_logo.bin")
    with urllib.request.urlopen(URL, timeout=60) as r:
        logo = parse(r.read().decode("latin-1"))
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    open(out, "wb").write(logo)
    print(f"{out}: boot logo downloaded")


if __name__ == "__main__":
    main()
