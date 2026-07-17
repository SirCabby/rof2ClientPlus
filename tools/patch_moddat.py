#!/usr/bin/env python3
"""Idempotently patch a RoF2 client's Resources/moddat.ini for the model-swap feature.

moddat.ini holds per-model-code camera tuning (first-person camera height `FPCOffset`, and a
`ZPCOffset` for the tall races). The classic-swap ALIAS codes (HU7/HU6/... built into the
rcp*.s3d, see build_models.py) inherit no offset from the client, so in first person the camera
sits at the wrong height for a swapped body (worst on troll/ogre). This appends a section per
alias code mirroring its native race's offsets. Without it, `make install-models` deploys the
models but first-person camera height is wrong on classic PC-race swaps.

Like patch_globalload.py: idempotent (a no-op once patched), backs the stock file up ONCE to
`moddat.ini.rcpbak`, preserves the file's CRLF, appends at end. Pure stdlib.

Usage: patch_moddat.py [--game-dir DIR] [--file PATH] [--revert] [--check]
"""
import os, sys

MARKER = "rof2ClientPlus: Luclin alias model codes"  # presence => already patched
# (code, FPCOffset, ZPCOffset|None) -- verbatim from the working 2026-07-16 client. Order and the
# absent HO7 are preserved so the output byte-matches. Values mirror the native race sections.
ALIAS_OFFSETS = [
    ("HU7", "0.5", None),  ("HU6", "0.5", None),
    ("BA7", "0.5", None),  ("BA6", "0.5", None),
    ("ER7", "0.5", None),  ("ER6", "0.5", None),
    ("EL7", "0.5", None),  ("EL6", "0.5", None),
    ("HI7", "0.5", None),  ("HI6", "0.5", None),
    ("DA7", "0.5", None),  ("DA6", "0.5", None),
    ("HA7", "0.5", None),  ("HA6", "0.5", None),
    ("DW7", "0.5", None),  ("DW6", "0.75", None),
    ("TR7", "1.1", "-1.0"), ("TR6", "1.1", "-1.0"),
    ("OG7", "2.35", "-2.0"), ("OG6", "2.75", "-2.0"),
    ("HO6", "0.5", None),
    ("GN7", "0.5", None),  ("GN6", "0.5", None),
]


def block_lines():
    lines = ["", "; " + MARKER + " mirror their native race sections"]
    for code, fpc, zpc in ALIAS_OFFSETS:
        lines.append(f"[{code}]")
        lines.append(f"FPCOffset={fpc}")
        if zpc is not None:
            lines.append(f"ZPCOffset={zpc}")
        lines.append("")
    return lines


def detect_eol(raw):
    return "\r\n" if b"\r\n" in raw else "\n"


def load(path):
    with open(path, "rb") as f:
        raw = f.read()
    eol = detect_eol(raw)
    lines = raw.decode("latin-1").split(eol)
    trailing_nl = bool(lines) and lines[-1] == ""
    if trailing_nl:
        lines = lines[:-1]
    return lines, eol, trailing_nl


def save(path, lines, eol, trailing_nl):
    body = eol.join(lines) + (eol if trailing_nl else "")
    with open(path, "wb") as f:
        f.write(body.encode("latin-1"))


def is_patched(lines):
    return any(MARKER in ln for ln in lines)


def main():
    a = sys.argv[1:]
    game = a[a.index("--game-dir") + 1] if "--game-dir" in a else "/home/joshua/Games/RoF2"
    path = a[a.index("--file") + 1] if "--file" in a else os.path.join(game, "Resources", "moddat.ini")
    if not os.path.isfile(path):
        raise SystemExit(f"ERROR: moddat.ini not found at {path}")
    lines, eol, tnl = load(path)

    if "--check" in a:
        print(f"{'PATCHED' if is_patched(lines) else 'STOCK'}: {path}")
        return
    if "--revert" in a:
        if not is_patched(lines):
            print(f">> {path} already stock; nothing to revert.")
            return
        # drop everything from the marker comment (and its leading blank) to EOF
        cut = next(i for i, ln in enumerate(lines) if MARKER in ln)
        if cut > 0 and lines[cut - 1] == "":
            cut -= 1
        save(path, lines[:cut], eol, tnl)
        print(f">> reverted alias offsets from {path}")
        return

    if is_patched(lines):
        print(f">> {path} already patched (found '{MARKER}'); no change.")
        return
    bak = path + ".rcpbak"
    if not os.path.exists(bak):
        save(bak, lines, eol, tnl)
        print(f">> backed up stock -> {bak}")
    save(path, lines + block_lines(), eol, tnl)
    print(f">> patched {path}: +{len(ALIAS_OFFSETS)} alias camera-offset sections")


if __name__ == "__main__":
    main()
