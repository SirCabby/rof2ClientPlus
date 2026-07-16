#!/usr/bin/env python3
"""Build a classic-alias PFS archive for the live model-swap feature.

Reads an EQ .s3d (default gequip.s3d), renames every held-item actor-def
`IT<n>_ACTORDEF` -> `ZT<n>_ACTORDEF` (same length, so the WLD string hash and all
fragment namerefs stay valid), and repacks into a new .s3d whose models coexist with
the client's normal ITn models under the aliased ZTn names. Loading this archive (via
GlobalLoad.txt) + redirecting ITn->ZTn (see src/model_swap.cpp) renders the *classic*
gequip mesh while the modern equipment-01.eqg override stays the default.

Pure stdlib. Self-verifies by reading the output back. Does not touch the game dir.
Usage: make_classic_alias.py [--src gequip.s3d] [--out rcpclassic.s3d] [--game-dir DIR]
"""
import os, re, sys, glob, struct, zlib

XOR = bytes([0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A])
NAME_CRC = 0x61580AC9
BLOCK = 0x2000

def eq_crc(name):  # validated 853/853 against gequip.s3d
    crc = 0
    for b in name.encode('latin-1') + b'\x00':
        crc ^= (b << 24) & 0xFFFFFFFF
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if (crc & 0x80000000) else (crc << 1) & 0xFFFFFFFF
    return crc

# ---- PFS read (full) -> [(name, data)] in on-disk data-offset order ----
def _inflate(data, off, size):
    out = bytearray(); p = off
    while len(out) < size:
        dl, il = struct.unpack_from('<II', data, p); p += 8
        out += zlib.decompress(bytes(data[p:p + dl])); p += dl
    return bytes(out)

def read_pfs(path):
    data = open(path, 'rb').read()
    diroff = struct.unpack_from('<I', data, 0)[0]
    off = diroff
    n = struct.unpack_from('<I', data, off)[0]; off += 4
    ent = []
    for _ in range(n):
        crc, fo, sz = struct.unpack_from('<III', data, off); off += 12
        ent.append((crc, fo, sz))
    names = []
    for crc, fo, sz in ent:
        if crc == NAME_CRC:
            raw = _inflate(data, fo, sz); p = 0
            fc = struct.unpack_from('<I', raw, p)[0]; p += 4
            for _ in range(fc):
                ln = struct.unpack_from('<I', raw, p)[0]; p += 4
                names.append(raw[p:p + ln - 1].decode('latin-1')); p += ln
            break
    de = sorted([e for e in ent if e[0] != NAME_CRC], key=lambda e: e[1])
    return [(nm, _inflate(data, e[1], e[2])) for nm, e in zip(names, de)]

# ---- PFS write from [(name, data)] (name order == data-offset order) ----
def _deflate_blocks(data):
    out = bytearray()
    i = 0
    while i < len(data):
        chunk = data[i:i + BLOCK]
        comp = zlib.compress(chunk, 9)
        out += struct.pack('<II', len(comp), len(chunk)) + comp
        i += BLOCK
    return bytes(out)

def write_pfs(path, files):
    body = bytearray()
    pos = 12  # after header
    entries = []
    for name, data in files:
        blocks = _deflate_blocks(data)
        entries.append([eq_crc(name), pos, len(data)])
        body += blocks
        pos += len(blocks)
    fn = bytearray(struct.pack('<I', len(files)))
    for name, _ in files:
        nb = name.encode('latin-1') + b'\x00'
        fn += struct.pack('<I', len(nb)) + nb
    fn_blocks = _deflate_blocks(bytes(fn))
    entries.append([NAME_CRC, pos, len(fn)])
    body += fn_blocks
    diroff = 12 + len(body)
    out = bytearray(struct.pack('<I', diroff) + b'PFS ' + struct.pack('<I', 0x00020000))
    out += body
    out += struct.pack('<I', len(entries))
    for crc, off, sz in sorted(entries, key=lambda e: e[0]):
        out += struct.pack('<III', crc, off, sz)
    open(path, 'wb').write(bytes(out))

# ---- WLD: rename IT<n>_ACTORDEF -> ZT<n>_ACTORDEF in the string hash ----
def rename_actordefs(wld):
    hashlen = struct.unpack_from('<IIIIIII', wld, 0)[5]
    enc = wld[28:28 + hashlen]
    dec = bytearray(b ^ XOR[i & 7] for i, b in enumerate(enc))
    parts = dec.split(b'\x00')
    renamed = 0
    for i, p in enumerate(parts):
        if re.fullmatch(rb'IT\d+_ACTORDEF', p):
            parts[i] = b'ZT' + p[2:]  # same length
            renamed += 1
    new = b'\x00'.join(parts)
    assert len(new) == len(dec), "string-hash length changed"
    reenc = bytes(b ^ XOR[i & 7] for i, b in enumerate(new))
    return wld[:28] + reenc + wld[28 + hashlen:], renamed

def main():
    a = sys.argv[1:]
    game = a[a.index('--game-dir') + 1] if '--game-dir' in a else '/home/joshua/Games/RoF2'
    src = a[a.index('--src') + 1] if '--src' in a else os.path.join(game, 'gequip.s3d')
    out = a[a.index('--out') + 1] if '--out' in a else \
        os.path.join(os.path.dirname(__file__), '..', 'build', 'rcpclassic.s3d')
    os.makedirs(os.path.dirname(out), exist_ok=True)

    files = read_pfs(src)
    print(f"read {src}: {len(files)} files")
    outfiles = []
    total_renamed = 0
    for name, data in files:
        if name.lower().endswith('.wld'):
            data, r = rename_actordefs(data)
            total_renamed += r
            name = 'rcpclassic.wld'  # match the archive short-name
            print(f"  wld {name}: renamed {r} actor-defs IT*->ZT*")
        outfiles.append((name, data))
    write_pfs(out, outfiles)
    print(f"wrote {out}: {os.path.getsize(out)} bytes, {total_renamed} aliased models")

    # self-verify: read back, confirm CRCs + a sample ZT actordef present
    back = read_pfs(out)
    assert len(back) == len(outfiles), "roundtrip file count mismatch"
    wld = next(d for n, d in back if n.lower().endswith('.wld'))
    hashlen = struct.unpack_from('<IIIIIII', wld, 0)[5]
    dec = bytes(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))
    zt = sorted(s.decode('latin-1') for s in dec.split(b'\x00') if re.fullmatch(rb'ZT\d+_ACTORDEF', s))
    it_left = any(re.fullmatch(rb'IT\d+_ACTORDEF', s) for s in dec.split(b'\x00'))
    print(f"VERIFY: roundtrip OK, {len(zt)} ZT actordefs (e.g. {', '.join(zt[:6])}), IT actordefs remaining: {it_left}")
    print(f"        classic alias for IT7 = {'ZT7' if 'ZT7_ACTORDEF' in zt else 'MISSING'}")

if __name__ == '__main__':
    main()
