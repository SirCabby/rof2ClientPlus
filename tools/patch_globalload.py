#!/usr/bin/env python3
"""Idempotently patch a RoF2 client's Resources/GlobalLoad.txt to load the mod's
classic-model archives (the rcp*.s3d built by build_models.py).

GlobalLoad.txt is the client's archive-load manifest. The stock file does NOT list
the rcp* archives, so without this patch the client never loads the classic meshes
and the model-swap feature silently renders stock (Luclin) models. `make install`
delivers the .asi/uifiles but CANNOT know the client's GlobalLoad.txt, so this is a
separate, explicit step (run by `make models`).

What it inserts (verified against the working 2026-07-15 client):
  * after the `gatesequip` equipment line: rcpclassic + the lgequip*/vequip archives
    (lgequip* un-gates the classic-helm/hair attach; see npc_model_swap.cpp).
  * after the `Global_chr` character line: rcpglobal_chr, the 3 creature archives,
    global5_chr/global5_chr2 (typed elementals), then the 24 per-race bodies
    (TFFTC = load-on-demand).

Idempotent: re-running is a no-op once patched. Backs the stock file up to
`GlobalLoad.txt.rcpbak` (once) before the first edit. Preserves the file's existing
line-ending style. Pure stdlib.

Usage:
  patch_globalload.py [--game-dir DIR] [--file PATH] [--revert] [--check]
"""
import os, sys

# --- the exact lines the mod adds, in order (no line endings; added at write time) ---
EQUIP_BLOCK = [
    "2,0,TFFFE,rcpclassic,Loading Character Equipment Files",
    "2,0,TFFFE,lgequip,Loading Character Equipment Files",
    "2,0,TFFFE,lgequip2,Loading Character Equipment Files",
    "2,0,TFFFE,lgequip_amr,Loading Character Equipment Files",
    "2,0,TFFFE,lgequip_amr2,Loading Character Equipment Files",
    "2,0,TFFFE,vequip,Loading Character Equipment Files",
]
CHR_BLOCK = [
    "3,0,TFFFC,rcpglobal_chr,Loading Characters",
    "3,0,TFFFC,rcpwolf,Loading Characters",
    "3,0,TFFFC,rcptiger,Loading Characters",
    "3,0,TFFFC,rcpdrake,Loading Characters",
    "3,0,TFFFC,global5_chr,Loading Characters",
    "3,0,TFFFC,global5_chr2,Loading Characters",
]
# 24 per-race bodies: male=7, female=6. Order matches the working client.
_RACES = ["hu", "ba", "er", "el", "hi", "da", "ha", "dw", "tr", "og", "ho", "gn"]
for _r in _RACES:
    CHR_BLOCK.append(f"3,0,TFFTC,rcp{_r}7,Loading Characters")
    CHR_BLOCK.append(f"3,0,TFFTC,rcp{_r}6,Loading Characters")

SENTINEL = "rcpclassic"          # presence => already patched
EQUIP_ANCHOR = "gatesequip"      # insert EQUIP_BLOCK after the line containing this
CHR_ANCHOR = "Global_chr"        # insert CHR_BLOCK after the "3,1,...,Global_chr,..." line


def load(path):
    # The stock RoF2 file ships with MIXED line endings (48 CRLF + 36 LF lines), so
    # per-file EOL detection can't round-trip it. We canonicalize to LF: splitlines()
    # here, save() joins with "\n". The client reads either form, output is
    # deterministic (byte-reproducible by build_dist.py), and the byte-true stock is
    # preserved by the .rcpbak backup, not by EOL bookkeeping. eol is kept in the
    # signature for compatibility and is always "\n".
    with open(path, "rb") as f:
        raw = f.read()
    lines = raw.decode("latin-1").splitlines()
    trailing_nl = raw.endswith((b"\n", b"\r"))
    return lines, "\n", trailing_nl


def save(path, lines, eol, trailing_nl):
    body = eol.join(lines) + (eol if trailing_nl else "")
    with open(path, "wb") as f:
        f.write(body.encode("latin-1"))


def is_patched(lines):
    return any(SENTINEL in ln for ln in lines)


def find_anchor(lines, needle, require_field=None):
    for i, ln in enumerate(lines):
        if needle in ln and (require_field is None or require_field in ln):
            return i
    return -1


def patch(lines):
    # find anchors (CHR anchor: the '3,1' Global_chr base-load line, not a '_chr2' etc.)
    ei = find_anchor(lines, EQUIP_ANCHOR)
    ci = -1
    for i, ln in enumerate(lines):
        parts = ln.split(",")
        if len(parts) >= 4 and parts[3].strip().lower() == "global_chr":
            ci = i
            break
    if ei < 0:
        raise SystemExit(f"ERROR: equip anchor '{EQUIP_ANCHOR}' not found; aborting (unexpected GlobalLoad.txt).")
    if ci < 0:
        raise SystemExit(f"ERROR: character anchor '{CHR_ANCHOR}' not found; aborting (unexpected GlobalLoad.txt).")
    # insert CHR first if it is after EQUIP so the earlier index stays valid; handle either order
    inserts = sorted([(ei + 1, EQUIP_BLOCK), (ci + 1, CHR_BLOCK)], key=lambda x: -x[0])
    for at, block in inserts:
        lines[at:at] = block
    return lines


def revert(lines):
    return [ln for ln in lines if not any(tok in ln for tok in ("rcpclassic", "rcpglobal_chr", "rcpwolf",
            "rcptiger", "rcpdrake")) and not (ln.split(",")[3:4] and ln.split(",")[3].strip().lower().startswith("rcp"))]


def main():
    a = sys.argv[1:]
    game = a[a.index("--game-dir") + 1] if "--game-dir" in a else "/home/joshua/Games/RoF2"
    path = a[a.index("--file") + 1] if "--file" in a else os.path.join(game, "Resources", "GlobalLoad.txt")
    if not os.path.isfile(path):
        raise SystemExit(f"ERROR: GlobalLoad.txt not found at {path}")
    lines, eol, tnl = load(path)

    if "--check" in a:
        print(f"{'PATCHED' if is_patched(lines) else 'STOCK'}: {path}")
        return

    if "--revert" in a:
        if not is_patched(lines):
            print(f">> {path} already stock; nothing to revert.")
            return
        save(path, revert(lines), eol, tnl)
        print(f">> reverted rcp lines from {path}")
        return

    if is_patched(lines):
        print(f">> {path} already patched (found '{SENTINEL}'); no change.")
        return
    bak = path + ".rcpbak"
    if not os.path.exists(bak):
        save(bak, lines, eol, tnl)
        print(f">> backed up stock -> {bak}")
    save(path, patch(lines), eol, tnl)
    print(f">> patched {path}: +{len(EQUIP_BLOCK)} equip + {len(CHR_BLOCK)} character lines "
          f"({len(_RACES)} races x2 + rcpclassic/rcpglobal_chr/creatures)")


if __name__ == "__main__":
    main()
