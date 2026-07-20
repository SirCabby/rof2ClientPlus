#!/usr/bin/env python3
"""Lower (or raise) a classic boat model inside a zone's `<zone>_chr.s3d`.

The RoF2 client computes a ship's waterline itself (server z is ignored for
boats), so ride height is a property of the model: shifting the mesh's
DMSPRITEDEF2 CenterOffset moves the rendered hull relative to the point the
client floats. The classic SHIP hull has a 4-unit draft below its origin and
the client visibly floats it ~4 high in freporte, hence the default -4.

In-place float edits only — fragment sizes, offsets and every other byte of
the wld are untouched. Idempotent: the stock archive is backed up once as
`<archive>.rcpbak`, and every run rebuilds from that backup, so re-running
with a different --dz re-tunes rather than accumulates. Install is atomic
(temp + rename) because a running client may have the archive open.

Usage:
  patch_ship_z.py [--game-dir DIR] [--zone freporte] [--model SHIP]
                  [--dz -4] [--list] [--dry-run]
"""
import argparse, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_classic_alias import read_pfs, write_pfs, XOR


def wld_mesh_centers(wld, model):
    """Yield (label, frag_offset, center_xyz) for each 0x36 owned by `model`."""
    fragcount, hashlen = struct.unpack_from('<IIIIIII', wld, 0)[2], \
        struct.unpack_from('<IIIIIII', wld, 0)[5]
    dec = bytes(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))

    def name_at(off):
        end = dec.find(b'\x00', off)
        return dec[off:end].decode('latin-1', 'replace') if 0 <= off < len(dec) else ''

    out = []
    p = 28 + hashlen
    for _ in range(fragcount):
        size, ftype = struct.unpack_from('<II', wld, p)
        body = p + 8
        if ftype == 0x36:
            nameref = struct.unpack_from('<i', wld, body)[0]
            label = name_at(-nameref) if nameref < 0 else ''
            if label == f'{model}_DMSPRITEDEF' or label.startswith(f'{model}_DMSPRITEDEF'):
                out.append((label, p, struct.unpack_from('<fff', wld, body + 24)))
        p = body + size
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--game-dir', default='/home/joshua/Games/RoF2')
    ap.add_argument('--zone', default='freporte')
    ap.add_argument('--model', default='SHIP')
    ap.add_argument('--dz', type=float, default=-4.0)
    ap.add_argument('--list', action='store_true', help='print current centers and exit')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()

    src = os.path.join(a.game_dir, f'{a.zone}_chr.s3d')
    bak = src + '.rcpbak'
    if not os.path.exists(src):
        sys.exit(f'missing {src}')

    base = bak if os.path.exists(bak) else src
    files = read_pfs(base)
    wldname = f'{a.zone}_chr.wld'
    try:
        widx = next(i for i, (n, _) in enumerate(files) if n == wldname)
    except StopIteration:
        sys.exit(f'{wldname} not found in {base}')
    wld = bytearray(files[widx][1])

    meshes = wld_mesh_centers(wld, a.model)
    if not meshes:
        sys.exit(f'no {a.model} 0x36 meshes in {wldname}')
    if a.list:
        for label, _, (cx, cy, cz) in meshes:
            print(f'{label}: center=({cx:.3f}, {cy:.3f}, {cz:.3f})   [{base}]')
        return

    for label, p, (cx, cy, cz) in meshes:
        struct.pack_into('<f', wld, p + 8 + 32, cz + a.dz)
        print(f'{label}: centerZ {cz:.3f} -> {cz + a.dz:.3f} (dz {a.dz:+g}, from '
              f'{"backup" if base == bak else "stock"})')

    if a.dry_run:
        print('dry-run: nothing written')
        return

    files[widx] = (wldname, bytes(wld))
    if not os.path.exists(bak):
        os.replace(src, bak)          # stock preserved once
        print(f'stock backed up -> {bak}')
    tmp = src + '.rcptmp'
    write_pfs(tmp, files)
    # self-verify before install: the target centers landed where expected
    got = wld_mesh_centers(dict(read_pfs(tmp))[wldname], a.model)
    for (label, _, (_, _, cz_new)), (_, _, (_, _, cz_old)) in zip(got, meshes):
        assert abs(cz_new - (cz_old + a.dz)) < 1e-4, f'verify failed on {label}'
    os.replace(tmp, src)
    print(f'installed {src}')


if __name__ == '__main__':
    main()
