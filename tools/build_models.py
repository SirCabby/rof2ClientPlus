#!/usr/bin/env python3
"""Regenerate ALL 29 classic-model archives (rcp*.s3d) for the model-swap feature,
reproducibly, from source .s3d archives. This is the batch driver over
`isolate_archive.py` -- the single source of truth for how each shipped archive was
built. (Recovered + verified 2026-07-17 from the Jul 13-15 model-swap session; the
per-race and rcpclassic outputs regenerate BYTE-IDENTICAL to the originally shipped
files, creatures actordef-identical.)

WHY THIS EXISTS: `make install` deploys the .asi + uifiles but NOT these 29 .s3d
(59 MB) and NOT the client's Resources/GlobalLoad.txt edit. Without a recorded recipe
the feature was un-reproducible -- the 24 per-race + 5 special archives were built by
hand, one isolate_archive.py run each, with per-file --prefix/--swap args that lived
only inside the built binaries. This driver + patch_globalload.py close that gap.

THE RECIPE (every output is one isolate_archive.py run; see RECIPE below):
  * rcpclassic    <- gequip.s3d (RoF2's own)         --prefix RCP           (classic held items; ITn -> RCPITn)
  * rcpglobal_chr <- global_chr.s3d (TAKP classic)   --prefix RCP + 26 swaps (base bodies: race M->9 / F->8, +SKE/WOE)
  * rcpwolf       <- warrens_chr.s3d (Trilogy)        --prefix RC2 --swap WOL=WO9
  * rcptiger      <- arena_chr.s3d (Trilogy)          --prefix RC3 --swap TIG=TI9
  * rcpdrake      <- cobaltscar_chr.s3d (Trilogy)     --prefix RC4 --swap DRK=DR9
  * rcp<race>7    <- global<race>m_chr.s3d (TAKP)     --prefix "" --swap <RACE>M=<RACE>7   (male,   12 races)
  * rcp<race>6    <- global<race>f_chr.s3d (TAKP)     --prefix "" --swap <RACE>F=<RACE>6   (female, 12 races)

SOURCE PROVENANCE (see the backup README): TAKP "PC V2.1c" global*_chr archives +
EQ-Trilogy warrens/arena/cobaltscar_chr + RoF2's own gequip.s3d. All are collected in
--src-dir (default: the local backup's model-inputs/). If that dir is ever lost, the
originals are re-obtainable (TAKP client zip + an EQ-Trilogy extract).

Pure stdlib. Usage:
  build_models.py                       # regenerate all 29 into build/, verify vs the backup
  build_models.py --install             # ...then deploy to $GAME_DIR + patch GlobalLoad.txt
  build_models.py --only rcpwolf,rcphu7 # rebuild a subset
  build_models.py --list                # print the recipe table and exit
Options: --src-dir DIR  --out-dir DIR  --game-dir DIR  --verify-against DIR  --no-verify
"""
import os, re, sys, struct, subprocess, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
ISOLATE = os.path.join(HERE, "isolate_archive.py")
PATCH_GL = os.path.join(HERE, "patch_globalload.py")
PATCH_MODDAT = os.path.join(HERE, "patch_moddat.py")
REPO = os.path.dirname(HERE)

DEFAULT_SRC = "/home/joshua/Games/RoF2-modelswap-backup/model-inputs"
DEFAULT_OUT = os.path.join(REPO, "build")
DEFAULT_GAME = "/home/joshua/Games/RoF2"
DEFAULT_REF = "/home/joshua/Games/RoF2-modelswap-backup/built-s3d"  # reference bytes for verify

# 12 Luclin-revamped playable races. male gender -> alias 7 (from global<code>m_chr),
# female -> alias 6 (from global<code>f_chr). Order matches the shipped GlobalLoad.txt.
RACES = ["HU", "BA", "ER", "EL", "HI", "DA", "HA", "DW", "TR", "OG", "HO", "GN"]

# rcpglobal_chr: classic global_chr with every playable race body remapped to the 8/9
# alias namespace (M->9, F->8) + skeleton (SKE->SK9) + spirit-wolf (WOE->WO8). Exact
# order preserved from the recovered command (matters only for reproducible bytes).
GLOBAL_SWAP = "SKE=SK9,WOE=WO8," + ",".join(f"{r}M={r}9,{r}F={r}8" for r in RACES)


def per_race_recipe():
    rows = []
    for r in RACES:
        lo = r.lower()
        rows.append((f"rcp{lo}7", f"global{lo}m_chr.s3d", "", f"{r}M={r}7"))
        rows.append((f"rcp{lo}6", f"global{lo}f_chr.s3d", "", f"{r}F={r}6"))
    return rows


# (output_basename, source_basename, prefix, swap)  -- swap "" = none
RECIPE = [
    ("rcpclassic",    "gequip.s3d",         "RCP", ""),
    ("rcpglobal_chr", "global_chr.s3d",     "RCP", GLOBAL_SWAP),
    ("rcpwolf",       "warrens_chr.s3d",    "RC2", "WOL=WO9"),
    ("rcptiger",      "arena_chr.s3d",      "RC3", "TIG=TI9"),
    ("rcpdrake",      "cobaltscar_chr.s3d", "RC4", "DRK=DR9"),
] + per_race_recipe()


def actordefs(path):
    sys.path.insert(0, HERE)
    from make_classic_alias import read_pfs, XOR
    wld = next(d for n, d in read_pfs(path) if n.lower().endswith(".wld"))
    hashlen = struct.unpack_from("<IIIIIII", wld, 0)[5]
    dec = bytes(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))
    return {s.decode("latin-1", "replace") for s in dec.split(b"\x00") if s.endswith(b"_ACTORDEF")}


def build_one(out, src_base, prefix, swap, src_dir, out_dir):
    src = os.path.join(src_dir, src_base)
    if not os.path.isfile(src):
        return False, f"source missing: {src}"
    outp = os.path.join(out_dir, out + ".s3d")
    cmd = [sys.executable, ISOLATE, "--src", src, "--out", outp, "--prefix", prefix]
    if swap:
        cmd += ["--swap", swap]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.isfile(outp):
        return False, (r.stderr or r.stdout).strip()[-200:]
    return True, outp


def verify_one(out, out_dir, ref_dir):
    outp = os.path.join(out_dir, out + ".s3d")
    refp = os.path.join(ref_dir, out + ".s3d")
    if not os.path.isfile(refp):
        return "no-ref"
    with open(outp, "rb") as f:
        a = f.read()
    with open(refp, "rb") as f:
        b = f.read()
    if a == b:
        return "byte-identical"
    try:
        return "actordef-identical" if actordefs(outp) == actordefs(refp) else "MISMATCH"
    except Exception as e:
        return f"verify-error:{e}"


def main():
    a = sys.argv[1:]
    def opt(name, default):
        return a[a.index(name) + 1] if name in a else default
    src_dir = opt("--src-dir", DEFAULT_SRC)
    out_dir = opt("--out-dir", DEFAULT_OUT)
    game = opt("--game-dir", DEFAULT_GAME)
    ref_dir = opt("--verify-against", DEFAULT_REF)
    do_verify = "--no-verify" not in a
    only = set(opt("--only", "").split(",")) - {""} if "--only" in a else None

    if "--list" in a:
        print(f"{'OUTPUT':16} {'SOURCE':22} {'PREFIX':7} SWAP")
        for out, src, pfx, swap in RECIPE:
            print(f"{out:16} {src:22} {repr(pfx):7} {swap[:60]}{'...' if len(swap) > 60 else ''}")
        print(f"\n{len(RECIPE)} archives. src-dir default: {DEFAULT_SRC}")
        return

    os.makedirs(out_dir, exist_ok=True)
    rows = [r for r in RECIPE if only is None or r[0] in only]
    if not rows:
        raise SystemExit(f"--only matched nothing; valid: {', '.join(r[0] for r in RECIPE)}")
    print(f">> regenerating {len(rows)} archive(s) from {src_dir} -> {out_dir}")
    ok, results = 0, []
    for out, src, pfx, swap in rows:
        built, info = build_one(out, src, pfx, swap, src_dir, out_dir)
        if not built:
            print(f"  FAIL {out}: {info}")
            results.append((out, "FAIL"))
            continue
        v = verify_one(out, out_dir, ref_dir) if do_verify else "(skipped)"
        flag = "" if v in ("byte-identical", "actordef-identical", "(skipped)", "no-ref") else "  <-- CHECK"
        print(f"  ok   {out:16} {os.path.getsize(info):>8} B  {v}{flag}")
        results.append((out, v))
        ok += 1
    bad = [o for o, v in results if v not in ("byte-identical", "actordef-identical", "(skipped)", "no-ref")]
    print(f">> built {ok}/{len(rows)}; verify: "
          f"{sum(1 for _, v in results if v == 'byte-identical')} byte-identical, "
          f"{sum(1 for _, v in results if v == 'actordef-identical')} actordef-identical"
          + (f", {len(bad)} NEED REVIEW: {bad}" if bad else ""))
    if ok != len(rows) or bad:
        raise SystemExit(1)

    if "--install" in a:
        if not os.path.isdir(game):
            raise SystemExit(f"ERROR: --game-dir '{game}' not found")
        print(f">> installing {len(rows)} .s3d into {game} (atomic)")
        for out, *_ in rows:
            s = os.path.join(out_dir, out + ".s3d")
            d = os.path.join(game, out + ".s3d")
            tmp = d + ".rcp-new"
            shutil.copyfile(s, tmp)
            os.replace(tmp, d)
        print(">> patching GlobalLoad.txt")
        subprocess.run([sys.executable, PATCH_GL, "--game-dir", game], check=True)
        print(">> patching moddat.ini (alias-model camera offsets)")
        subprocess.run([sys.executable, PATCH_MODDAT, "--game-dir", game], check=True)
        print(">> done. Relaunch the client for the classic models to load.")


if __name__ == "__main__":
    main()
