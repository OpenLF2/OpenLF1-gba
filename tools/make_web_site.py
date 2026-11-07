#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Assembles the static site that builds the ROM in the browser.

    make_web_site.py ROM ELF ASSETDIR OUTDIR

ROM is the finished build/openlf1.gba, ELF the build/openlf1.elf it came from and ASSETDIR the build/assets. OUTDIR
gets the page, the converter's Python files, config.json, manifest.json and template.bin.gz: the ROM with the
converted data (pictures, sounds, ...) and the boot logo zeroed out. The browser runs the same converter, checks every
output file against the SHA-256 in manifest.json and writes it back at its ROM offset, so the result is the same file
the Makefile produces. The template holds only compiled code.

Set NM to the ARM toolchain's nm (default arm-none-eabi-nm).
"""
import gzip
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ROM_BASE = 0x08000000


def sha(b):
    return hashlib.sha256(b).hexdigest()


def symbols(elf):
    out = subprocess.run([os.environ.get("NM", "arm-none-eabi-nm"), elf], check=True, capture_output=True, text=True)
    return {line.split()[2]: int(line.split()[0], 16) for line in out.stdout.splitlines() if len(line.split()) == 3}


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    rom_path, elf, assets, out = sys.argv[1:]
    rom = bytearray(open(rom_path, "rb").read())
    syms = symbols(elf)
    os.makedirs(out, exist_ok=True)

    logo = bytes(rom[0x04:0xA0])
    rom[0x04:0xA0] = bytes(len(logo))
    manifest = {"logo": {"offset": 4, "sha256": sha(logo)}, "romSize": len(rom), "blobs": [], "headers": [],
                "python": []}
    for name in sorted(os.listdir(assets)):
        data = open(os.path.join(assets, name), "rb").read()
        if name.endswith(".bin"):
            offset = syms[name.replace(".", "_")] - ROM_BASE
            if rom[offset:offset + len(data)] != data:
                raise SystemExit(f"{name} is not in the ROM at {offset:#x}")
            rom[offset:offset + len(data)] = bytes(len(data))
            manifest["blobs"].append({"name": name, "offset": offset, "size": len(data), "sha256": sha(data)})
        elif name != "assets.S":
            manifest["headers"].append({"name": name, "sha256": sha(data)})

    os.makedirs(os.path.join(out, "py"), exist_ok=True)
    for name in sorted(os.listdir(HERE)):
        if name.endswith(".py") and name != os.path.basename(__file__):
            shutil.copy(os.path.join(HERE, name), os.path.join(out, "py", name))
            manifest["python"].append(name)
    for name in ("index.html", "worker.js", "config.json"):
        shutil.copy(os.path.join(ROOT, "web", name), os.path.join(out, name))
    with gzip.GzipFile(os.path.join(out, "template.bin.gz"), "wb", compresslevel=9, mtime=0) as f:
        f.write(rom)
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, separators=(",", ":"))
    print(f"template.bin.gz: {os.path.getsize(os.path.join(out, 'template.bin.gz')) / 1024:.0f} KiB, "
          f"{len(manifest['blobs'])} data files")


if __name__ == "__main__":
    main()
