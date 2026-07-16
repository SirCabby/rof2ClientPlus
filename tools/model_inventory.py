#!/usr/bin/env python3
"""Generate an inventory of the RoF2 client's 3D model assets, flagging which
WEAPON (IT##) and NPC/CHARACTER models have a newer archive shadowing a classic
one -- i.e. "replaced".

A model is *replaced* when the same model code is defined in more than one loaded
archive; the higher-priority archive wins, so the classic look is hidden. The two
priority rules the client uses:
  * an .eqg definition beats an .s3d definition of the same name, and
  * within a format, the archive loaded later in Resources/GlobalLoad.txt wins.

Weapon meshes  = IT## actor defs inside gequip*.s3d (classic/Luclin .wld) and
                 it#####.mod files inside *equip*.eqg (per-expansion overrides).
Char/NPC meshes = <race><gender>/<npc> actor defs inside global*_chr.s3d and the
                 393 <zone>_chr.s3d, plus per-model <code>.eqg archives.

Reads GAME_DIR from ../config.mk (or --game-dir). Writes markdown (default
tools/model_inventory.md). Pure stdlib; does not modify game files.

Usage: python3 tools/model_inventory.py [--game-dir DIR] [--out FILE] [--no-zones]
"""
import os, re, sys, glob, struct, zlib, datetime

# ----------------------------------------------------------------------------- PFS (.s3d/.eqg) reader
def _inflate(data, offset, inflated_size):
    out = bytearray(); pos = offset
    while len(out) < inflated_size:
        dl, il = struct.unpack_from('<II', data, pos); pos += 8
        out += zlib.decompress(bytes(data[pos:pos + dl])); pos += dl
    return bytes(out)

def _read_pfs(path):
    """Return (names[], (data, offset_by_name{})) so callers can list or extract."""
    with open(path, 'rb') as f:
        data = f.read()
    dir_off = struct.unpack_from('<I', data, 0)[0]
    off = dir_off
    count = struct.unpack_from('<I', data, off)[0]; off += 4
    entries = []
    for _ in range(count):
        crc, foff, size = struct.unpack_from('<III', data, off); off += 12
        entries.append((crc, foff, size))
    names = []
    for crc, foff, size in entries:
        if crc == 0x61580AC9:  # filename pseudo-file
            raw = _inflate(data, foff, size); p = 0
            fc = struct.unpack_from('<I', raw, p)[0]; p += 4
            for _ in range(fc):
                ln = struct.unpack_from('<I', raw, p)[0]; p += 4
                names.append(raw[p:p + ln - 1].decode('latin-1', 'replace')); p += ln
            break
    data_entries = sorted([e for e in entries if e[0] != 0x61580AC9], key=lambda e: e[1])
    by_name = {}
    sizes = {}
    if len(names) == len(data_entries):
        for nm, e in zip(names, data_entries):
            by_name[nm.lower()] = (e[1], e[2]); sizes[nm.lower()] = e[2]
    return names, data, by_name

def pfs_names(path):
    names, _, _ = _read_pfs(path)
    return names

def pfs_extract(path, member):
    names, data, by_name = _read_pfs(path)
    hit = by_name.get(member.lower())
    if not hit:
        return None
    return _inflate(data, hit[0], hit[1])

# ----------------------------------------------------------------------------- WLD model-name table
WLD_KEY = bytes([0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A])

def wld_actordefs(wld):
    hashlen = struct.unpack_from('<IIIIIII', wld, 0)[5]
    enc = wld[28:28 + hashlen]
    dec = bytes(b ^ WLD_KEY[i & 7] for i, b in enumerate(enc))
    strings = [s.decode('latin-1', 'replace') for s in dec.split(b'\x00') if s]
    return sorted({s[:-len('_ACTORDEF')] for s in strings if s.endswith('_ACTORDEF')})

def s3d_codes(path):
    try:
        names = pfs_names(path)
        wldname = next((n for n in names if n.lower().endswith('.wld')), None)
        if not wldname:
            return []
        wld = pfs_extract(path, wldname)
        return wld_actordefs(wld) if wld else []
    except Exception as e:
        sys.stderr.write(f"  ! {os.path.basename(path)}: {e}\n")
        return []

# ----------------------------------------------------------------------------- names
RACE2 = {'HU': 'Human', 'BA': 'Barbarian', 'ER': 'Erudite', 'EL': 'Wood Elf',
         'HI': 'High Elf', 'DA': 'Dark Elf', 'HA': 'Half Elf', 'DW': 'Dwarf',
         'TR': 'Troll', 'OG': 'Ogre', 'HO': 'Halfling', 'GN': 'Gnome',
         'IK': 'Iksar', 'KE': 'Vah Shir', 'FR': 'Froglok'}
# Curated model-code -> creature/model name (NPC codes are single-gender 3-letter tags).
# Best-effort community mapping: deities/named models from bonzz.com/eqzoo, the rest from
# established EQ model-code convention + zone context. Blank = not confidently known --
# left blank rather than guessed, since the code->race bridge is client-side (not in the DB).
FIXED = {
    # deities / avatars (bonzz.com/eqzoo, authoritative)
    'CAZ': 'Cazic-Thule', 'CTH': 'Cazic-Thule', 'INN': 'Innoruuk', 'BER': 'Bertoxxulous',
    'BRE': 'Brell Serilis', 'BRI': 'Bristlebane', 'ERO': 'Erollisi Marr', 'FEN': 'Fennin Ro',
    'KAR': 'Karana',
    # undead / summoned
    'SKE': 'Skeleton', 'SKT': 'Skeleton', 'FSK': 'Froglok Skeleton', 'IKS': 'Iksar Skeleton',
    'ZOM': 'Zombie', 'GHU': 'Ghoul', 'MUM': 'Mummy', 'SPE': 'Spectre', 'WIL': 'Will-o-Wisp',
    'GOL': 'Golem', 'GAR': 'Gargoyle', 'GRG': 'Gargoyle',
    # humanoids / monsters
    'ORC': 'Orc', 'ORK': 'Orc', 'GOB': 'Goblin', 'GNN': 'Gnoll', 'KOB': 'Kobold', 'IMP': 'Imp',
    'MIN': 'Minotaur', 'GIA': 'Giant', 'BIX': 'Bixie', 'CEN': 'Centaur', 'CNT': 'Centaur',
    'WER': 'Werewolf', 'DJI': 'Djinn', 'EFR': 'Efreeti', 'HAR': 'Harpy', 'SPH': 'Sphinx',
    'PEG': 'Pegasus', 'GRI': 'Griffon',
    # animals
    'RAT': 'Rat', 'SNA': 'Snake', 'BAT': 'Bat', 'SPI': 'Spider', 'WOL': 'Wolf', 'BEA': 'Bear',
    'CUB': 'Bear Cub', 'TIG': 'Tiger', 'PUM': 'Puma', 'AVI': 'Aviak', 'WAS': 'Wasp',
    'BET': 'Beetle', 'ALL': 'Alligator', 'ARM': 'Armadillo', 'ANK': 'Ankylosaurus', 'FIS': 'Fish',
    # elementals
    'ELE': 'Elemental', 'AEL': 'Air Elemental', 'EEL': 'Earth Elemental', 'FEL': 'Fire Elemental',
    # dragons / reptiles
    'DRG': 'Dragon', 'DRA': 'Drake', 'DRK': 'Drake', 'CHM': 'Chimera',
    # misc classic
    'EYE': 'Eye of Zomm', 'BOAT': 'Boat', 'FROGLOK': 'Froglok', 'GEN': 'Generic held item',
}

def code_name(code):
    c = code.upper()
    if c in FIXED:
        return FIXED[c]
    if len(c) == 3 and c[2] in 'MF' and c[:2] in RACE2:
        return f"{RACE2[c[:2]]} {'male' if c[2] == 'M' else 'female'}"
    return ''

# ----------------------------------------------------------------------------- helpers
def base(p):
    return os.path.basename(p)

def it_of(s):
    m = re.match(r'(?i)it0*(\d+)', s)
    return int(m.group(1)) if m else None

def find_game_dir(cli):
    if cli:
        return cli
    cfg = os.path.join(os.path.dirname(__file__), '..', 'config.mk')
    if os.path.exists(cfg):
        for line in open(cfg):
            m = re.match(r'\s*GAME_DIR\s*:?=\s*(.+?)\s*$', line)
            if m:
                return m.group(1)
    return '/home/joshua/Games/RoF2'

def md_table(headers, rows):
    out = ['| ' + ' | '.join(headers) + ' |',
           '|' + '|'.join('---' for _ in headers) + '|']
    for r in rows:
        out.append('| ' + ' | '.join(str(c) for c in r) + ' |')
    return '\n'.join(out)

# ----------------------------------------------------------------------------- DB item names
# Weapon/item IT## codes map directly to item names via items.idfile. Read the EQEmu DB
# through the akk-stack MariaDB container. Credentials are read from akk-stack/.env at
# RUNTIME (never stored here); if the container/.env is absent the report degrades to
# code-only for weapons.
def _parse_env(path):
    if not path or not os.path.exists(path):
        return None
    v = {}
    for line in open(path):
        m = re.match(r'\s*([A-Z0-9_]+)=(.*)', line)
        if m:
            v[m.group(1)] = m.group(2).strip().strip('"').strip("'")
    if 'MARIADB_USER' in v and 'MARIADB_PASSWORD' in v:
        return {'user': v['MARIADB_USER'], 'pw': v['MARIADB_PASSWORD'],
                'db': v.get('MARIADB_DATABASE', 'peq')}
    return None

def load_item_names(env_path, container):
    """{itnum -> (count, 'ex1 / ex2 / ex3')} from items.idfile, or {} if DB unavailable."""
    import subprocess
    creds = _parse_env(env_path)
    if not creds:
        sys.stderr.write(f"  (no DB creds at {env_path}; weapons stay code-only)\n")
        return {}
    sql = ("SELECT idfile, COUNT(*), "
           "SUBSTRING_INDEX(GROUP_CONCAT(DISTINCT name ORDER BY CHAR_LENGTH(name),name "
           "SEPARATOR ' / '),' / ',3) FROM items WHERE idfile RLIKE '^IT[0-9]+$' "
           "GROUP BY idfile")
    try:
        p = subprocess.run(['docker', 'exec', '-e', f"MYSQL_PWD={creds['pw']}", container,
                            'mysql', '-u', creds['user'], creds['db'], '-N', '-e', sql],
                           capture_output=True, text=True, timeout=30)
    except Exception as e:
        sys.stderr.write(f"  (DB query skipped: {e})\n")
        return {}
    if p.returncode != 0:
        sys.stderr.write(f"  (DB query failed: {p.stderr.strip()[:160]})\n")
        return {}
    d = {}
    for line in p.stdout.splitlines():
        parts = line.split('\t')
        if len(parts) >= 3:
            n = it_of(parts[0])
            if n is not None:
                d[n] = (int(parts[1]), parts[2])
    sys.stderr.write(f"  DB: named {len(d)} IT item models\n")
    return d

# ----------------------------------------------------------------------------- main
def main():
    args = sys.argv[1:]
    game = find_game_dir(args[args.index('--game-dir') + 1] if '--game-dir' in args else None)
    out = args[args.index('--out') + 1] if '--out' in args else \
        os.path.join(os.path.dirname(__file__), 'model_inventory.md')
    do_zones = '--no-zones' not in args
    akk_env = args[args.index('--akk-env') + 1] if '--akk-env' in args else \
        os.path.expanduser('~/workspace/GitHub/akk-stack/.env')
    db_container = args[args.index('--db-container') + 1] if '--db-container' in args else \
        'akk-stack-mariadb-1'
    if not os.path.isdir(game):
        sys.exit(f"GAME_DIR not found: {game}")
    G = lambda pat: sorted(glob.glob(os.path.join(game, pat)))
    log = lambda m: sys.stderr.write(m + '\n')

    # item names from the EQEmu DB (weapons: items.idfile -> name); {} if unavailable
    item_names = {} if '--no-db' in args else load_item_names(akk_env, db_container)
    def wname(n):
        hit = item_names.get(n)
        return hit[1] if hit else ''

    # ---- WEAPONS -------------------------------------------------------------
    log("scanning weapon archives ...")
    weap_s3d = {}   # archive -> {itnum}
    for p in G('gequip*.s3d'):
        weap_s3d[base(p)] = {n for n in (it_of(c) for c in s3d_codes(p)) if n is not None}
    weap_eqg = {}   # archive -> {itnum}
    for p in G('*equip*.eqg'):
        nums = set()
        for n in pfs_names(p):
            if n.lower().endswith('.mod'):
                v = it_of(n)
                if v is not None:
                    nums.add(v)
        if nums:
            weap_eqg[base(p)] = nums

    CLASSIC_W = ('gequip.s3d', 'gequip2.s3d')
    itnum_src = {}  # itnum -> {'s3d':[...], 'eqg':[...]}
    for a, nums in weap_s3d.items():
        for n in nums:
            itnum_src.setdefault(n, {'s3d': [], 'eqg': []})['s3d'].append(a)
    for a, nums in weap_eqg.items():
        for n in nums:
            itnum_src.setdefault(n, {'s3d': [], 'eqg': []})['eqg'].append(a)

    def classify_w(src):
        s3d, eqg = src['s3d'], src['eqg']
        classic = [a for a in s3d if a in CLASSIC_W]
        if eqg and s3d:
            return 'REPLACED (.eqg over .s3d)'
        if len(s3d) > 1:
            return 'REPLACED (later .s3d)'
        if eqg and not s3d:
            return 'eqg-only (new item)'
        if classic:
            return 'classic only'
        return 'modern .s3d only'

    weap_replaced = sorted(n for n, s in itnum_src.items()
                           if classify_w(s).startswith('REPLACED'))

    # ---- CHARACTERS / NPCs ---------------------------------------------------
    log("scanning global character archives ...")
    glob_chr = {}   # archive -> [codes]
    for p in G('global*_chr.s3d'):
        glob_chr[base(p)] = s3d_codes(p)

    CLASSIC_C = 'global_chr.s3d'
    classic_codes = set(glob_chr.get(CLASSIC_C, []))
    code_src = {}   # code -> [archives] (global only)
    for a, codes in glob_chr.items():
        for c in codes:
            code_src.setdefault(c, []).append(a)

    # per-model .eqg character/creature archives (have skeletal .mds or animation .ani)
    log("scanning character .eqg archives (this reads many TOCs) ...")
    eqg_chr = {}    # archive -> [model basenames]
    for p in G('*.eqg'):
        try:
            names = pfs_names(p)
        except Exception:
            continue
        low = [n.lower() for n in names]
        if any(n.endswith('.mds') for n in low) or any(n.endswith('.ani') for n in low):
            models = sorted({os.path.splitext(n)[0].upper()
                             for n in names if n.lower().endswith(('.mds', '.mod'))})
            if models:
                eqg_chr[base(p)] = models

    # replaced player/creature models: any code defined in >1 archive (a newer one wins).
    # base = the shared global_chr / globalN_chr archive; override = the per-race globalXXX_chr
    # (loaded on demand by the UseLuclin<Race><Sex> toggles) or a later shared archive.
    SHARED_RE = re.compile(r'^global\d*_chr\.s3d$')
    char_replaced = []
    for c in sorted(code_src):
        arcs = sorted(code_src[c])
        if len(arcs) <= 1:
            continue
        base_arc = [a for a in arcs if a == CLASSIC_C] or [a for a in arcs if SHARED_RE.match(a)]
        override = [a for a in arcs if a not in base_arc]
        char_replaced.append((c, code_name(c), sorted(base_arc), sorted(override)))

    # ---- ZONE NPCs -----------------------------------------------------------
    zone_codes = {}   # zone archive -> [codes]
    if do_zones:
        zones = [p for p in G('*_chr.s3d') if not base(p).startswith('global')]
        log(f"scanning {len(zones)} zone NPC archives ...")
        for i, p in enumerate(zones):
            if i % 75 == 0:
                log(f"  zone {i}/{len(zones)}")
            zone_codes[base(p)] = s3d_codes(p)

    # ---- WRITE REPORT --------------------------------------------------------
    log(f"writing {out} ...")
    today = datetime.date.today().isoformat()
    L = []
    L.append("# RoF2 client model inventory — classic vs. replaced\n")
    L.append(f"*Generated {today} by `tools/model_inventory.py` from `{game}`.*")
    L.append("*Regenerate: `python3 tools/model_inventory.py`. A model is **replaced** when its "
             "code is defined in more than one loaded archive — the higher-priority one wins "
             "(`.eqg` beats `.s3d`; later `GlobalLoad.txt` entry beats earlier), hiding the classic look.*\n")

    # name coverage
    all_npc_codes = set(code_src) | {c for cs in zone_codes.values() for c in cs}
    npc_named = sum(1 for c in all_npc_codes if code_name(c))
    weap_named = sum(1 for n in itnum_src if wname(n))

    # summary
    L.append("## Summary\n")
    L.append(md_table(["", "count"], [
        ["gequip*.s3d weapon archives", len(weap_s3d)],
        ["*equip*.eqg override archives", len(weap_eqg)],
        ["distinct IT weapon models", len(itnum_src)],
        ["&nbsp;&nbsp;named from DB", f"{weap_named}" + ("" if item_names else " (DB off)")],
        ["**weapon models REPLACED**", f"**{len(weap_replaced)}**"],
        ["global*_chr.s3d archives", len(glob_chr)],
        ["distinct global char/NPC codes", len(code_src)],
        ["**global models REPLACED**", f"**{len(char_replaced)}**"],
        ["character/creature .eqg archives", len(eqg_chr)],
        ["zone _chr.s3d archives scanned", len(zone_codes)],
        ["distinct NPC codes (global+zone)", len(all_npc_codes)],
        ["&nbsp;&nbsp;named (best-effort map)", f"{npc_named} / {len(all_npc_codes)}"],
    ]))
    L.append("")

    db_note = (" Item names come from the EQEmu DB (`items.idfile`) — one model backs many items, "
               "so the 2–3 shortest examples are shown." if item_names else
               " (DB unavailable — names omitted; see https://www.eqemulator.org/forums/showthread.php?t=2538)")
    # weapons replaced
    L.append("## Weapons — replaced models\n")
    L.append("Classic `IT##` weapon meshes that a higher-priority archive overrides." + db_note + "\n")
    rows = []
    for n in weap_replaced:
        s = itnum_src[n]
        rows.append([f"IT{n}", wname(n) or '?', ', '.join(sorted(s['s3d'])) or '—',
                     ', '.join(sorted(s['eqg'])) or '(later .s3d)'])
    L.append(md_table(["model", "in-game name (examples)", "classic source (.s3d)", "overridden by"],
                      rows) if rows else "_none detected_")
    L.append("")

    # full weapon index
    L.append("## Weapons — full IT index\n")
    rows = []
    for n in sorted(itnum_src):
        s = itnum_src[n]
        rows.append([f"IT{n}", wname(n) or '', ', '.join(sorted(s['s3d'])) or '—',
                     ', '.join(sorted(s['eqg'])) or '—', classify_w(s)])
    L.append(md_table(["model", "in-game name (examples)", ".s3d archives", ".eqg archives", "status"], rows))
    L.append("")

    # global char replaced
    L.append("## Characters & NPCs — replaced global models\n")
    L.append("Codes defined in more than one archive — a newer one wins, hiding the classic look. "
             "The base is the shared `global[N]_chr.s3d`; the override is the per-race "
             "`global<race><gender>_chr.s3d` (loaded on demand by the `UseLuclin<Race><Sex>` "
             "`eqclient.ini` toggles) or a later shared archive. Creatures like skeletons are "
             "force-loaded via `GlobalLoad.txt`.\n")
    rows = [[c, nm or '', ', '.join(basea) or '—', ', '.join(over)] for c, nm, basea, over in char_replaced]
    L.append(md_table(["code", "name", "classic / base", "overridden by"], rows) if rows else "_none detected_")
    L.append("")

    # global char full index
    L.append("## Characters & NPCs — global model index\n")
    rows = []
    for c in sorted(code_src):
        arcs = sorted(code_src[c])
        tag = 'REPLACED' if len(arcs) > 1 else \
              ('classic' if c in classic_codes else 'new-only')
        rows.append([c, code_name(c) or '', ', '.join(arcs), tag])
    L.append(md_table(["code", "name", "archives", "status"], rows))
    L.append("")

    # NPC code legend (every distinct global+zone code -> best-effort name + reach)
    L.append("## NPC model-code legend\n")
    L.append(f"Every distinct NPC model code across the global and zone archives ({len(all_npc_codes)} "
             f"total; {npc_named} named). Names are a best-effort map — player races are exact, "
             "deities are from bonzz.com/eqzoo, common creatures from EQ code convention; blanks are "
             "codes not confidently identified (the code→creature bridge is client-side, not in the DB). "
             "`#zones` = how many zone archives bundle the model.\n")
    zone_freq = {}
    for codes in zone_codes.values():
        for c in codes:
            zone_freq[c] = zone_freq.get(c, 0) + 1
    rows = []
    for c in sorted(all_npc_codes):
        rows.append([c, code_name(c) or '', zone_freq.get(c, 0),
                     'global' if c in code_src else ''])
    L.append(md_table(["code", "name", "#zones", "global?"], rows))
    L.append("")

    # eqg character models
    if eqg_chr:
        L.append("## Characters & NPCs — per-model `.eqg` archives\n")
        L.append("Later-expansion creature/character models shipped as `.eqg`. A code that also "
                 "appears in an `.s3d` archive above is an `.eqg`-over-`.s3d` replacement.\n")
        rows = []
        for a in sorted(eqg_chr):
            models = eqg_chr[a]
            overlap = sorted(set(models) & set(code_src))
            rows.append([a, ', '.join(models[:12]) + (' …' if len(models) > 12 else ''),
                         ', '.join(overlap) if overlap else '—'])
        L.append(md_table(["archive", "models (.mod/.mds)", "also in .s3d (replaces)"], rows))
        L.append("")

    # zone index
    if zone_codes:
        L.append("## Zone NPC index\n")
        L.append("NPC model codes unique to each zone archive. A code also found in a newer global "
                 "archive (e.g. `global6_chr.s3d` Luclin creatures) is marked ⚠ — its classic zone "
                 "version is shadowed.\n")
        newer_global = set()
        for a, codes in glob_chr.items():
            if a != CLASSIC_C:
                newer_global |= set(codes)
        rows = []
        for z in sorted(zone_codes):
            codes = zone_codes[z]
            marked = ', '.join((f"{c}⚠" if c in newer_global else c) for c in codes)
            rows.append([z.replace('_chr.s3d', ''), len(codes), marked])
        L.append(md_table(["zone", "#", "NPC codes"], rows))
        L.append("")

    with open(out, 'w') as f:
        f.write('\n'.join(L))
    log(f"done: {out}  ({len(weap_replaced)} weapons, {len(char_replaced)} global chars replaced)")

if __name__ == '__main__':
    main()
