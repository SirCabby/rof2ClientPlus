#!/usr/bin/env python3
"""Build rcppc_chr.s3d: stock global_chr with classic PC-race head meshes promoted to
toggleable dag PIECES so the 2013 client's native SwapHead works.

WHY: classic PC skeletons carry their head (XXXHE00) only in the 0x10 fragment's
attached-SKIN list; XXXHE01-03 (leather/chain/plate helm heads) are orphan meshes.
CActorInstance::SwapHead toggles def PIECES by name ('OGMHE03_DMSPRITEDEF') — pieces are
born only from dags that carry a mesh ref (the working creature template: IVMHE00_DAG).
With no HE pieces, every classic PC helm swap is a silent name-miss => helms never render.

WHAT: full copy of global_chr.s3d (fragment indices untouched — the isolate_archive.py
lesson), with per PC skeleton (12 races x 2 genders):
  - 4 new dags XXXHE00..03_DAG parented under XXXHE_DAG, each with an identity 1-frame
    trackdef/track and a 0x2D ref to the existing XXXHEnn_DMSPRITEDEF mesh (IVM pattern);
  - the XXXHE00 entry removed from the attached-skins list (else double-draw);
  - only the XXX_ACTORDEF / XXX_HS_DEF labels renamed (same length: male XX2, female XX3)
    so the archive coexists with stock Global_chr; ALL interior names stay plain so
    SwapHead's race-code-built piece names strcmp-match and stock animations bind by name.
The mod's resolver detour redirects race-code defs (OGM -> OG2) to these.

Pure stdlib. Self-verifies (reparse + roundtrip). Usage:
  make_pc_heads.py [--src .../global_chr.s3d] [--out build/rcppc_chr.s3d]
                   [--piece-name-source mesh|dag]  (default mesh: dag named XXXHEnn_DAG;
                    'dag' names the dag XXXHEnn_DMSPRITEDEF itself, if the client derives
                    piece names from dag labels rather than the referenced mesh)
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_classic_alias import read_pfs, write_pfs, XOR

PC = ['HUM', 'HUF', 'BAM', 'BAF', 'ERM', 'ERF', 'ELM', 'ELF', 'HIM', 'HIF', 'DAM', 'DAF',
      'HAM', 'HAF', 'DWM', 'DWF', 'TRM', 'TRF', 'OGM', 'OGF', 'HOM', 'HOF', 'GNM', 'GNF']
HE_RANGE = range(4)  # HE00..HE03

def alias(code):
    """same-length rename: male XXM->XX2, female XXF->XX3"""
    return code[:2] + ('2' if code[2] == 'M' else '3')

def parse_wld(wld):
    hdr = struct.unpack_from('<IIIIIII', wld, 0)
    assert hdr[0] == 0x54503D02, 'not a WLD'
    nfrag, hashlen = hdr[2], hdr[5]
    dec = bytearray(b ^ XOR[i & 7] for i, b in enumerate(wld[28:28 + hashlen]))
    frags = []  # [fid, bytearray(data)]
    off = 28 + hashlen
    for _ in range(nfrag):
        size, fid = struct.unpack_from('<II', wld, off)
        frags.append([fid, bytearray(wld[off + 8: off + 8 + size])])
        off += 8 + size
    tail = wld[off:]  # 0x33-style footer bytes if any
    return hdr, dec, frags, tail

def emit_wld(hdr, dec, frags, tail):
    h = list(hdr)
    h[2] = len(frags)
    h[5] = len(dec)
    out = bytearray(struct.pack('<IIIIIII', *h))
    out += bytes(b ^ XOR[i & 7] for i, b in enumerate(bytes(dec)))
    for fid, d in frags:
        out += struct.pack('<II', len(d), fid) + d
    out += tail
    return bytes(out)

class Hash:
    def __init__(self, dec):
        self.dec = dec
        self.by_name = {}
        pos = 0
        while pos < len(dec):
            end = dec.find(b'\x00', pos)
            if end < 0:
                break
            if end > pos:
                self.by_name[bytes(dec[pos:end])] = pos
            pos = end + 1

    def name_at(self, off):
        end = self.dec.find(b'\x00', off)
        return bytes(self.dec[off:end])

    def rename(self, old, new):
        assert len(old) == len(new)
        off = self.by_name.pop(old)
        self.dec[off:off + len(old)] = new
        self.by_name[new] = off
        return off

    def add(self, name):
        assert name not in self.by_name, f'hash string exists: {name}'
        off = len(self.dec)
        self.dec += name + b'\x00'
        self.by_name[name] = off
        return off

def frag_label(hash_, frag):
    fid, d = frag
    if len(d) < 4:
        return b''
    nr = struct.unpack_from('<i', d, 0)[0]
    return hash_.name_at(-nr) if nr < 0 else b''

class Skel:
    """parsed 0x10 fragment"""
    def __init__(self, d):
        self.nameref, = struct.unpack_from('<i', d, 0)
        o = 4
        self.flags, self.ndags = struct.unpack_from('<Ii', d, o); o += 8
        self.colref, = struct.unpack_from('<i', d, o); o += 4
        self.center = self.radius = None
        if self.flags & 1:
            self.center = d[o:o + 12]; o += 12
        if self.flags & 2:
            self.radius = d[o:o + 4]; o += 4
        self.dags = []
        for _ in range(self.ndags):
            dn, df, tr, mr, ns = struct.unpack_from('<iIiii', d, o); o += 20
            subs = list(struct.unpack_from(f'<{ns}i', d, o)); o += 4 * ns
            self.dags.append([dn, df, tr, mr, subs])
        self.skins = []
        if self.flags & 0x200:
            nsk, = struct.unpack_from('<i', d, o); o += 4
            refs = struct.unpack_from(f'<{nsk}i', d, o); o += 4 * nsk
            links = struct.unpack_from(f'<{nsk}i', d, o); o += 4 * nsk
            self.skins = [list(t) for t in zip(refs, links)]
        assert o == len(d), f'0x10 parse consumed {o} != {len(d)}'

    def emit(self):
        out = bytearray(struct.pack('<iIi i'.replace(' ', ''), self.nameref, self.flags,
                                    len(self.dags), self.colref))
        if self.flags & 1:
            out += self.center
        if self.flags & 2:
            out += self.radius
        for dn, df, tr, mr, subs in self.dags:
            out += struct.pack('<iIiii', dn, df, tr, mr, len(subs))
            out += struct.pack(f'<{len(subs)}i', *subs)
        if self.flags & 0x200:
            out += struct.pack('<i', len(self.skins))
            out += struct.pack(f'<{len(self.skins)}i', *[s[0] for s in self.skins])
            out += struct.pack(f'<{len(self.skins)}i', *[s[1] for s in self.skins])
        return out

# identity 1-frame trackdef: flags=8 (per IVMHE00_TRACKDEF), frame = quat(16384,0,0,0)
# shift (0,0,0)/256 — transform is irrelevant for bone-assigned t36 meshes, identity is
# the safe value either way.
def trackdef_bytes(nameref):
    frame = struct.pack('<8h', 16384, 0, 0, 0, 0, 0, 0, 256)
    return struct.pack('<iII', nameref, 8, 1) + frame

def build(src, out, piece_name_source='mesh', rename=True, mode='dags'):
    files = read_pfs(src)
    wldname = next(n for n, _ in files if n.lower().endswith('.wld'))
    wld = next(d for n, d in files if n == wldname)
    hdr, dec, frags, tail = parse_wld(wld)
    hash_ = Hash(dec)

    # index fragments by label
    by_label = {}
    for i, fr in enumerate(frags):
        lbl = frag_label(hash_, fr)
        if lbl:
            by_label.setdefault(lbl, i + 1)  # 1-based

    patched = []
    for code in PC:
        c = code.encode()
        skel_idx = by_label.get(c + b'_HS_DEF')
        if not skel_idx:
            print(f'  !! {code}: no skeleton, skipped')
            continue
        fid, d = frags[skel_idx - 1]
        assert fid == 0x10
        sk = Skel(d)
        # parent dag: XXXHE_DAG
        parent = next((i for i, dg in enumerate(sk.dags)
                       if hash_.name_at(-dg[0]) == c + b'HE_DAG'), None)
        assert parent is not None, f'{code}: no HE_DAG'
        if mode == 'skins':
            # SKIN-APPEND MODE (2026-07-28, post loader-RE): piece records are born from the
            # attached-skins list (expanded t36 run), NOT from dags. HE00 is already a skin (real
            # geometry); append HE01-03 as skins with the same dag-link so the loader builds them
            # exactly like HE00. All four join the head mask by name shape; the first SetHead
            # hides all-but-one. No new dags/tracks/hash strings needed.
            he00_idx = by_label.get(c + b'HE00_DMSPRITEDEF')
            he00_link = None
            for ref, link in sk.skins:
                tgt = struct.unpack_from('<i', frags[ref - 1][1], 4)[0] if 0 < ref <= len(frags) else 0
                if tgt == he00_idx:
                    he00_link = link
            assert he00_link is not None, f'{code}: HE00 not in skin list'
            added = []
            for n in (1, 2, 3):
                mesh_lbl = c + b'HE%02d_DMSPRITEDEF' % n
                mesh_idx = by_label.get(mesh_lbl)
                if not mesh_idx:
                    print(f'  !! {code}: missing {mesh_lbl.decode()}, variant skipped')
                    continue
                frags.append([0x2D, bytearray(struct.pack('<iiI', 0, mesh_idx, 0))])
                sk.skins.append([len(frags), he00_link])
                added.append(n)
            frags[skel_idx - 1][1] = sk.emit()
            if rename:
                al = alias(code).encode()
                hash_.rename(c + b'_ACTORDEF', al + b'_ACTORDEF')
                hash_.rename(c + b'_HS_DEF', al + b'_HS_DEF')
            patched.append(code)
            print(f'  {code}{"->" + alias(code) if rename else ""}: +{len(added)} head skins '
                  f'(link=dag[{he00_link}])')
            continue
        added = []
        for n in HE_RANGE:
            mesh_lbl = c + b'HE%02d_DMSPRITEDEF' % n
            mesh_idx = by_label.get(mesh_lbl)
            if not mesh_idx:
                print(f'  !! {code}: missing {mesh_lbl.decode()}, variant skipped')
                continue
            stem = c + b'HE%02d' % n
            dag_name = mesh_lbl if piece_name_source == 'dag' else stem + b'_DAG'
            # new fragments appended at end; indices are 1-based positions
            td_ref = hash_.add(stem + b'_TRACKDEF')
            frags.append([0x12, bytearray(trackdef_bytes(-td_ref))])
            td_idx = len(frags)
            tr_ref = hash_.add(stem + b'_TRACK')
            frags.append([0x13, bytearray(struct.pack('<iiI', -tr_ref, td_idx, 0))])
            tr_idx = len(frags)
            frags.append([0x2D, bytearray(struct.pack('<iiI', 0, mesh_idx, 0))])
            mr_idx = len(frags)
            if dag_name in hash_.by_name:  # 'dag' mode reuses the mesh label bytes
                dg_ref = hash_.by_name[dag_name]
            else:
                dg_ref = hash_.add(dag_name)
            sk.dags.append([-dg_ref, 0, tr_idx, mr_idx, []])
            added.append(len(sk.dags) - 1)
        sk.dags[parent][4].extend(added)
        # drop the HE00 skin entry (mesh now renders as the HE00 piece)
        he00_idx = by_label.get(c + b'HE00_DMSPRITEDEF')
        kept = []
        for ref, link in sk.skins:
            tgt = struct.unpack_from('<i', frags[ref - 1][1], 4)[0] if 0 < ref <= len(frags) else 0
            if tgt == he00_idx:
                continue
            kept.append([ref, link])
        removed = len(sk.skins) - len(kept)
        sk.skins = kept
        frags[skel_idx - 1][1] = sk.emit()
        # optionally rename actordef + skeleton label (same length; interior names untouched).
        # --no-rename keeps plain names so the archive OVERRIDES stock Global_chr's defs by
        # GlobalLoad position (registry first/last-wins decides which side of Global_chr).
        if rename:
            al = alias(code).encode()
            hash_.rename(c + b'_ACTORDEF', al + b'_ACTORDEF')
            hash_.rename(c + b'_HS_DEF', al + b'_HS_DEF')
        patched.append(code)
        print(f'  {code}{"->" + alias(code) if rename else ""}: +{len(added)} head dags '
              f'(parent dag[{parent}]), -{removed} head skin')

    new_wld = emit_wld(hdr, hash_.dec, frags, tail)
    out_files = [(n, new_wld if n == wldname else d) for n, d in files]
    os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
    write_pfs(out, out_files)

    # ---- self-verify: reparse the emitted archive ----
    v_files = read_pfs(out)
    v_wld = next(d for n, d in v_files if n == wldname)
    v_hdr, v_dec, v_frags, _ = parse_wld(v_wld)
    v_hash = Hash(v_dec)
    v_by_label = {}
    for i, fr in enumerate(v_frags):
        lbl = frag_label(v_hash, fr)
        if lbl:
            v_by_label.setdefault(lbl, i + 1)
    for code in patched:
        c = code.encode()
        al = alias(code).encode() if rename else c
        assert al + b'_ACTORDEF' in v_hash.by_name, f'{code}: actordef missing'
        if rename:
            assert c + b'_ACTORDEF' not in v_hash.by_name, f'{code}: plain actordef leaked'
        sk = Skel(v_frags[v_by_label[al + b'_HS_DEF'] - 1][1])
        if mode == 'skins':
            seen = set()
            for ref, link in sk.skins:
                tgt = struct.unpack_from('<i', v_frags[ref - 1][1], 4)[0]
                lbl = frag_label(v_hash, v_frags[tgt - 1])
                if lbl.startswith(c + b'HE0'):
                    seen.add(int(lbl[len(c) + 2:len(c) + 4]))
            assert seen == {0, 1, 2, 3}, f'{code}: head skins {sorted(seen)}'
            continue
        heads = 0
        for dn, df, tr, mr, subs in sk.dags:
            nm = v_hash.name_at(-dn)
            if nm.startswith(c + b'HE0') and mr:
                tgt = struct.unpack_from('<i', v_frags[mr - 1][1], 4)[0]
                tlbl = frag_label(v_hash, v_frags[tgt - 1])
                assert tlbl == c + b'HE%02d_DMSPRITEDEF' % heads, \
                    f'{code}: HE{heads} mesh ref -> {tlbl}'
                heads += 1
        assert heads == 4, f'{code}: {heads}/4 head pieces'
        for ref, link in sk.skins:
            tgt = struct.unpack_from('<i', v_frags[ref - 1][1], 4)[0]
            assert frag_label(v_hash, v_frags[tgt - 1]) != c + b'HE00_DMSPRITEDEF', \
                f'{code}: head skin still attached'
    # untouched fragments byte-identical
    o_hdr, o_dec, o_frags, _ = parse_wld(wld)
    same = sum(1 for a, b in zip(o_frags, v_frags) if a[1] == b[1])
    print(f'verify OK: {len(patched)} skeletons patched, {len(v_frags) - len(o_frags)} '
          f'fragments appended, {same}/{len(o_frags)} original fragments byte-identical')
    return patched

def main():
    a = sys.argv[1:]
    src = a[a.index('--src') + 1] if '--src' in a else '/home/joshua/Games/RoF2/global_chr.s3d'
    out = a[a.index('--out') + 1] if '--out' in a else \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'rcppc_chr.s3d')
    pns = a[a.index('--piece-name-source') + 1] if '--piece-name-source' in a else 'mesh'
    rename = '--no-rename' not in a
    mode = a[a.index('--mode') + 1] if '--mode' in a else 'dags'
    print(f'{src} -> {out} (mode={mode}, piece names from {pns}, rename={rename})')
    build(src, os.path.abspath(out), pns, rename, mode)
    print(f'wrote {out}: {os.path.getsize(os.path.abspath(out))} bytes')

if __name__ == '__main__':
    main()
