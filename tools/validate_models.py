#!/usr/bin/env python3
"""Validate creature model-swap candidates against the LIVE server DB + the client
race->code table + a classic (Trilogy) reference, so we only add catalog entries that
are (a) actually used by a spawning NPC and (b) genuinely revamped (modern mesh differs
from the classic one). This is the tool behind the 2026-07-14 finding that WOE=spirit
wolf (not yeti), DRK is a no-op (identical classic/modern), and BEA/IMP/SCA aren't revamps.

Three data sources, joined:
  1. eqgame.exe    -> race->model-code map (parsed from the 0x50a440 registration run).
  2. peq MariaDB   -> spawn counts + sample NPC names per race (what actually spawns).
  3. *_chr.s3d     -> mesh vert counts, RoF2 (modern) vs Trilogy (classic) = revamp check.

Usage:
  validate_models.py                 # audit the current catalog codes
  validate_models.py ALL WOF BEA     # audit specific model codes
  validate_models.py --candidates    # rank every global6/global2 code by live spawns

Paths assume the RoF2 install + akk-stack + extracted Trilogy in their usual spots (below).
"""
import struct, sys, os, re, glob, subprocess, json

ROF = '/home/joshua/Games/RoF2'
EXE = f'{ROF}/eqgame.exe'
TRIL_GLOB = '/home/joshua/Downloads/eqtrilogy_extract/**/*_chr.s3d'
ENV = '/home/joshua/workspace/GitHub/akk-stack/.env'
DB_CONTAINER = 'akk-stack-mariadb-1'
CATALOG = ['SKE', 'SKT', 'WOL', 'TIG', 'WOE']  # DRK removed 2026-07-14 (no revamp)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wld_mesh_preview import extract_wld, wld_strings, frag_iter, parse_mesh_36

# --- 1. client race->code map (registration run: push codeptr; push 2; push race; call 0x50a440) ---
# race is imm8 (6a XX) when <128 else imm32 (68 ....). .rdata VA 0x9c0000 @ file 0x5be800.
def race_code_map():
    d = open(EXE, 'rb').read()
    def read_code(va):
        if not (0x9c0000 <= va < 0xaa7000):
            return None
        b = d[va - 0x9c0000 + 0x5be800:va - 0x9c0000 + 0x5be800 + 4]
        s = b.rstrip(b'\x00')
        if 2 <= len(s) <= 3 and all(65 <= c <= 90 or 48 <= c <= 57 for c in s):
            return s.decode()
        return None
    r2c = {}
    i = -1
    while True:
        i = d.find(b'\x6a\x02', i + 1)
        if i < 0:
            break
        if d[i - 5] != 0x68:
            continue
        c = read_code(struct.unpack_from('<I', d, i - 4)[0])
        if not c:
            continue
        p = i + 2
        race = d[p + 1] if d[p] == 0x6a else (struct.unpack_from('<I', d, p + 1)[0] if d[p] == 0x68 else None)
        if race is not None and race < 3000:
            r2c.setdefault(race, c)
    c2r = {}
    for r, c in r2c.items():
        c2r.setdefault(c, []).append(r)
    return r2c, c2r

# --- 2. DB spawn counts per race (via docker exec) ---
def db_spawns():
    pw = None
    for ln in open(ENV):
        if ln.startswith('MARIADB_PASSWORD='):
            pw = ln.strip().split('=', 1)[1]
    q = ("SELECT nt.race, COUNT(DISTINCT s2.id), COUNT(DISTINCT nt.id), "
         "SUBSTRING_INDEX(GROUP_CONCAT(DISTINCT nt.name ORDER BY nt.id SEPARATOR '|'),'|',4) "
         "FROM npc_types nt JOIN spawnentry se ON se.npcID=nt.id JOIN spawngroup sg ON sg.id=se.spawngroupID "
         "JOIN spawn2 s2 ON s2.spawngroupID=sg.id GROUP BY nt.race;")
    out = subprocess.run(['docker', 'exec', DB_CONTAINER, 'mysql', '-ueqemu', f'-p{pw}', 'peq', '-N', '-e', q],
                         capture_output=True, text=True).stdout
    sp = {}
    for ln in out.splitlines():
        p = ln.split('\t')
        if len(p) >= 4 and p[0].isdigit():
            sp[int(p[0])] = (int(p[1]), int(p[2]), p[3])
    return sp

# --- 3. mesh vert count for a code in an archive (sum of its 0x36 pieces) ---
def verts(arc, code):
    try:
        wld = extract_wld(arc)
    except Exception:
        return None
    _, o2s = wld_strings(wld)
    v = 0
    found = False
    for i, t, nr, b, l in frag_iter(wld):
        if nr < 0:
            nm = o2s.get(-nr, '')
            if nm == code + '_ACTORDEF':
                found = True
            if t == 0x36 and nm.startswith(code):
                v += parse_mesh_36(wld, b)[3][0]
    return v if found else None

def modern_verts(code):
    for g in ('global6_chr.s3d', 'global2_chr.s3d', 'global_chr.s3d'):
        v = verts(f'{ROF}/{g}', code)
        if v:
            return v, g
    return None, None

def classic_verts(code):
    # classic art is either in RoF2's own global_chr (both-resident codes like SKE/WOE) or in a
    # pre-Luclin Trilogy archive (zone-sourced codes like WOL/TIG/ALL/WOF).
    best = None
    gc = verts(f'{ROF}/global_chr.s3d', code)
    if gc:
        best = ('global_chr.s3d', gc)
    for t in glob.glob(TRIL_GLOB, recursive=True):
        v = verts(t, code)
        if v and (best is None or v > best[1]):
            best = (os.path.basename(t), v)
    return best

def audit(codes, c2r, sp):
    print(f"  {'code':4}{'race(s)':10}{'spawns':>7}{'types':>6}  {'modern':>7}{'classic':>9}  verdict   samples")
    for c in codes:
        races = c2r.get(c, [])
        spp = sum(sp[r][0] for r in races if r in sp)
        ty = sum(sp[r][1] for r in races if r in sp)
        nm = []
        for r in races:
            if r in sp:
                nm += sp[r][2].split('|')
        mv, _ = modern_verts(c)
        cv = classic_verts(c)
        cvv = cv[1] if cv else None
        if not races:
            verdict = 'NO RACE'
        elif spp == 0:
            verdict = 'unused'
        elif mv and cvv and mv == cvv:
            verdict = 'NO REVAMP'
        elif mv and cvv:
            verdict = 'REVAMP ok'
        else:
            verdict = 'need-src' if mv else '?'
        samp = ', '.join(list(dict.fromkeys(nm))[:3])
        print(f"  {c:4}{str(races):10}{spp:7}{ty:6}  {str(mv):>7}{str(cvv):>9}  {verdict:9} {samp}")

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    r2c, c2r = race_code_map()
    sp = db_spawns()
    if '--candidates' in sys.argv:
        # every code carried by a Luclin-add archive (global6/global2), ranked by live spawns
        import re as _re
        cand = set()
        for g in ('global6_chr.s3d', 'global2_chr.s3d'):
            _, o2s = wld_strings(extract_wld(f'{ROF}/{g}'))
            for i, t, nr, b, l in frag_iter(extract_wld(f'{ROF}/{g}')):
                if nr < 0:
                    m = _re.match(r'^([A-Z]{2,4})_ACTORDEF$', o2s.get(-nr, ''))
                    if m:
                        cand.add(m.group(1))
        codes = sorted(cand, key=lambda c: -sum(sp[r][0] for r in c2r.get(c, []) if r in sp))
    else:
        codes = args or CATALOG
    audit(codes, c2r, sp)

if __name__ == '__main__':
    main()
