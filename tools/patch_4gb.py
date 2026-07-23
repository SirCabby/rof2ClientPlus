#!/usr/bin/env python3
"""Set (or clear) LARGE_ADDRESS_AWARE on the RoF2 eqgame.exe -- the standard "4GB patch".

eqgame.exe is a 32-bit process; without IMAGE_FILE_LARGE_ADDRESS_AWARE (0x0020 in the PE
COFF Characteristics) it can address only 2 GB, which a modded client (extra archives,
big UI overrides, many zones loaded over a session) can exhaust and crash. With the flag,
64-bit Windows and Wine give it the full 4 GB. The patch is one flag bit, applied in
place -- byte-identical to what the common third-party "4gb_patch" tools produce.

Idempotent + reversible:
  * first run saves the pristine exe once as eqgame.exe.pre4gb
  * --check  reports the current state and exits (no writes)
  * --revert restores the .pre4gb backup over eqgame.exe
Install is atomic (write temp in the same dir, then rename) so a running client keeps
its intact mapping.

Usage:
  patch_4gb.py [--game-dir DIR | --file EXE] [--check | --revert]
"""
import argparse
import os
import shutil
import struct
import sys

LAA = 0x0020  # IMAGE_FILE_LARGE_ADDRESS_AWARE


def characteristics_offset(data: bytes) -> int:
    """Return the byte offset of the 2-byte COFF Characteristics field, validating PE headers."""
    if data[:2] != b"MZ":
        raise SystemExit("ERROR: not an MZ executable")
    (e_lfanew,) = struct.unpack_from("<I", data, 0x3C)
    if data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
        raise SystemExit("ERROR: PE signature not found")
    return e_lfanew + 4 + 18  # PE sig + COFF header offsetof(Characteristics)


def read_flags(path: str) -> tuple[int, int]:
    with open(path, "rb") as f:
        data = f.read(0x400)
    off = characteristics_offset(data)
    (flags,) = struct.unpack_from("<H", data, off)
    return off, flags


def atomic_patch(path: str, off: int, flags: int) -> None:
    with open(path, "rb") as f:
        data = bytearray(f.read())
    struct.pack_into("<H", data, off, flags)
    tmp = path + ".4gb-new"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--game-dir", default=".", help="RoF2 client dir containing eqgame.exe")
    ap.add_argument("--file", help="patch this exe instead of <game-dir>/eqgame.exe")
    ap.add_argument("--check", action="store_true", help="report state, change nothing")
    ap.add_argument("--revert", action="store_true", help="restore the .pre4gb backup")
    a = ap.parse_args()

    exe = a.file or os.path.join(a.game_dir, "eqgame.exe")
    bak = exe + ".pre4gb"
    if not os.path.isfile(exe):
        raise SystemExit(f"ERROR: {exe} not found (set --game-dir or --file)")

    off, flags = read_flags(exe)
    state = "SET (4GB-aware)" if flags & LAA else "not set (2GB limit)"
    print(f">> {exe}: Characteristics=0x{flags:04X} at 0x{off:X} -- LARGE_ADDRESS_AWARE {state}")

    if a.check:
        return
    if a.revert:
        if not os.path.isfile(bak):
            raise SystemExit(f"ERROR: no backup to revert ({bak})")
        shutil.copy2(bak, exe + ".4gb-new")
        os.replace(exe + ".4gb-new", exe)
        print(f">> reverted from {bak}")
        return
    if flags & LAA:
        print(">> already patched; nothing to do")
        return
    if not os.path.isfile(bak):
        shutil.copy2(exe, bak)
        print(f">> saved pristine exe -> {bak}")
    atomic_patch(exe, off, flags | LAA)
    print(">> patched: LARGE_ADDRESS_AWARE set (revert with --revert)")


if __name__ == "__main__":
    main()
