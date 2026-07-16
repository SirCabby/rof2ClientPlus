#!/usr/bin/env python3
"""Build a namespace-ISOLATED classic-model archive for the live model-swap feature.

The full-copy alias archive (make_classic_alias.py) rendered classic geometry but with
WRONG textures: classic weapon meshes share material/texture LABELS (MACETOP, MACEHANDLE,
HOTPINK, ...) with other models, and duplicating them collided in the client's global
material/texture pool. Fix: copy gequip.wld but rename EVERY fragment label (its name-hash
entry) with a unique prefix, so nothing collides -- while leaving fragment INDICES, geometry,
and the inline .bmp filenames untouched (textures still resolve from the shared, identical
.bmp pool). No renumbering => none of the 0x14/0x31/0x04 count-field or 0x03 inline traps.

An item's held model tag ITn thus becomes <prefix>ITn (redirect ITn -> <prefix>ITn).

Pure stdlib; self-verifies (roundtrip + the renamed mesh still parses).
Usage: isolate_archive.py [--src gequip.s3d] [--out build/rcpclassic.s3d] [--prefix RCP]
"""
import os, re, sys, struct
sys.path.insert(0, os.path.dirname(__file__))
from make_classic_alias import read_pfs, write_pfs, XOR  # reuse validated PFS I/O

def rebuild_hash_rename(wld, prefix, swap=None):
    hdr = struct.unpack_from('<IIIIIII', wld, 0)
    fragcount, hashlen = hdr[2], hdr[5]
    dec = bytes(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))
    # index the string hash: offset -> bytes (each null-terminated)
    strings, pos = [], 0
    while pos < len(dec):
        end = dec.find(b'\x00', pos)
        if end < 0:
            end = len(dec)
        strings.append((pos, dec[pos:end]))
        pos = end + 1
    off_set = {o for o, _ in strings}

    # a fragment's OWN label is the nameref at body+0 (negative -> string offset). Collect those
    # offsets so we rename only real fragment labels (not referenced specials like SPRITECALLBACK).
    label_offsets = set()
    p = 28 + hashlen
    for _ in range(fragcount):
        size = struct.unpack_from('<I', wld, p)[0]
        body = p + 8
        nameref = struct.unpack_from('<i', wld, body)[0]
        if nameref < 0 and -nameref in off_set:
            label_offsets.add(-nameref)
        p = body + size

    # rebuild the hash: prefix labels, keep everything else; map old offset -> new offset
    pfx = prefix.encode('latin-1')
    # Optional same-length in-place code swap (e.g. SKE->SK9). Creature ANIMATION tracks are named
    # <anim-code><model-code><bone>_TRACK (L01SKE.., D05SKE..); a prefix would move the model code out
    # of position so the client's <anim><model> lookup misses => T-pose. An in-place same-length swap
    # keeps the model code where every label (mesh AND animation) expects it, and leaves offsets fixed.
    swp = [(f.encode('latin-1'), t.encode('latin-1')) for f, t in (swap or [])]
    oldoff_to_new, newparts, newoff, renamed = {}, [], 0, 0
    for off, s in strings:
        # The same-length code swap applies to EVERY hash string, label or not: new-style (Luclin) skeletons
        # pair animation tracks to bones BY NAME via dag-name strings that are NOT fragment own-labels
        # (e.g. HUMPEBIP01_DAG); leaving those unswapped half-renames the pairing -> T-pose, and leaks
        # bare-code names into the global pool (collides with the native model). Same-length swaps keep
        # every string offset usable even for in-body references the nameref remap below doesn't cover.
        ns, swapped = s, False
        for f, t in swp:
            if f in ns:
                ns = ns.replace(f, t)
                swapped = True
        if off in label_offsets and s:
            if not swapped:
                ns = pfx + ns  # code-free label -> per-archive prefix (unique namespace)
            renamed += 1
        oldoff_to_new[off] = newoff
        newparts.append(ns)
        newoff += len(ns) + 1
    new_dec = b'\x00'.join(newparts) + b'\x00'
    while len(new_dec) % 4:
        new_dec += b'\x00'
    new_enc = bytes(b ^ XOR[i & 7] for i, b in enumerate(new_dec))

    # copy fragment region; remap each fragment's body+0 nameref (+ 0x14's body+8 callback ref).
    # ALSO: 0x03 (BMINFO) fragments carry XOR-encoded INLINE texture filenames (humch0101.dds ...). Luclin
    # armor/hair textures are CODE-KEYED FILES the client resolves by constructing "<code><part><mat><pc>"
    # names from the model code -- so the inline names (and the PFS file entries, see main) must get the
    # same-length swap too, case-insensitively, or an alias body has no armor/hair textures.
    swp_ci = []
    for f, t in swp:
        swp_ci += [(f.upper(), t.upper()), (f.lower(), t.lower())]
    frag = bytearray(wld[28 + hashlen:])
    p = 0
    inline_swapped = 0
    for _ in range(fragcount):
        size, typ = struct.unpack_from('<II', frag, p)
        body = p + 8
        nameref = struct.unpack_from('<i', frag, body)[0]
        if nameref < 0 and -nameref in oldoff_to_new:
            struct.pack_into('<i', frag, body, -oldoff_to_new[-nameref])
        if typ == 0x14:
            cb = struct.unpack_from('<i', frag, body + 8)[0]
            if cb < 0 and -cb in oldoff_to_new:
                struct.pack_into('<i', frag, body + 8, -oldoff_to_new[-cb])
        if typ == 0x03 and swp_ci:
            # body: [i32 nameref][i32 count-1] then per name: [u16 len][len XOR-encoded chars]
            q = body + 8
            n = struct.unpack_from('<i', frag, body + 4)[0] + 1
            for _n in range(max(n, 1)):
                if q + 2 > body + size:
                    break
                ln = struct.unpack_from('<H', frag, q)[0]
                raw = bytes(frag[q + 2:q + 2 + ln])
                dec2 = bytes(b ^ XOR[i & 7] for i, b in enumerate(raw))
                nd = dec2
                for f, t in swp_ci:
                    nd = nd.replace(f, t)
                if nd != dec2:
                    frag[q + 2:q + 2 + ln] = bytes(b ^ XOR[i & 7] for i, b in enumerate(nd))
                    inline_swapped += 1
                q += 2 + ln
        p = body + size
    new_wld = wld[:20] + struct.pack('<I', len(new_dec)) + wld[24:28] + new_enc + bytes(frag)
    return new_wld, renamed, inline_swapped

def actordefs(wld):
    hashlen = struct.unpack_from('<IIIIIII', wld, 0)[5]
    dec = bytes(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))
    return {s.decode('latin-1', 'replace') for s in dec.split(b'\x00') if s.endswith(b'_ACTORDEF')}

def main():
    a = sys.argv[1:]
    game = a[a.index('--game-dir') + 1] if '--game-dir' in a else '/home/joshua/Games/RoF2'
    src = a[a.index('--src') + 1] if '--src' in a else os.path.join(game, 'gequip.s3d')
    out = a[a.index('--out') + 1] if '--out' in a else \
        os.path.join(os.path.dirname(__file__), '..', 'build', 'rcpclassic.s3d')
    prefix = a[a.index('--prefix') + 1] if '--prefix' in a else 'RCP'
    swap = []
    if '--swap' in a:
        for pair in a[a.index('--swap') + 1].split(','):  # e.g. --swap SKE=SK9,WOE=WO8 (same-length swaps)
            f, t = pair.split('=')
            if len(f) != len(t):
                raise SystemExit('--swap codes must be the same length')
            swap.append((f, t))
    os.makedirs(os.path.dirname(out), exist_ok=True)

    files = read_pfs(src)
    outfiles, renamed, inline_swapped, files_renamed = [], 0, 0, 0
    for name, data in files:
        if name.lower().endswith('.wld'):
            data, renamed, inline_swapped = rebuild_hash_rename(data, prefix, swap)
            name = os.path.splitext(os.path.basename(out))[0] + '.wld'  # inner .wld matches the .s3d base
        else:
            # code-keyed texture FILES (humch0101.dds ...) must carry the swap too -- the client constructs
            # these names from the model code at armor/hair apply time (see rebuild_hash_rename 0x03 note).
            nn = name
            for f, t in swap or []:
                nn = nn.replace(f.lower(), t.lower()).replace(f.upper(), t.upper())
            if nn != name:
                files_renamed += 1
                name = nn
        outfiles.append((name, data))
    write_pfs(out, outfiles)
    print(f"wrote {out}: {os.path.getsize(out)} bytes, {renamed} labels prefixed '{prefix}', "
          f"{inline_swapped} inline texture names + {files_renamed} file entries swapped")

    # self-verify: read back; confirm prefixed actordefs, no bare IT#_ACTORDEF, mesh still parses
    back = read_pfs(out)
    wld = next(d for n, d in back if n.lower().endswith('.wld'))
    ad = actordefs(wld)
    bare_it = sorted(x for x in ad if re.fullmatch(r'IT\d+_ACTORDEF', x))
    pref_it7 = f"{prefix}IT7_ACTORDEF"
    print(f"VERIFY: {len(ad)} actordefs, {pref_it7} present={pref_it7 in ad}, "
          f"bare IT#_ACTORDEF leaked={len(bare_it)}")
    # geometry intact? parse the renamed mesh
    try:
        from wld_mesh_preview import find_mesh, parse_mesh_36
        _, body, _ = find_mesh(wld, f"{prefix}IT7_DMSPRITEDEF")
        if body is not None:
            v, f, c, st = parse_mesh_36(wld, body)
            print(f"        {prefix}IT7_DMSPRITEDEF parses: {st[0]} verts, {st[1]} polys (classic = 47/19)")
    except Exception as e:
        print(f"        mesh check skipped: {e}")

if __name__ == '__main__':
    main()
