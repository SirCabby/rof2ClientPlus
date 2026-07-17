# Model-swap assets — reproduce & restore

The model-swap feature (`/rcpmodels`, `/rcppc`, `/rcpbody`, the Options **Models** tab) renders
**classic** EQ meshes in place of the client's Luclin ones. It depends on runtime data that is
**not** in git and **not** delivered by `make install`:

1. **29 `rcp*.s3d` classic-model archives** (~59 MB) dropped in the game root.
2. An edit to **`$GAME_DIR/Resources/GlobalLoad.txt`** so the client actually loads them
   (the mod DLL never writes this file).
3. An edit to **`$GAME_DIR/Resources/moddat.ini`** appending a first-person camera offset
   (`FPCOffset`/`ZPCOffset`) section per alias model code, so the camera height is right on
   classic PC-race swaps (worst on troll/ogre).

All three are fully reproducible from source. This document is the recipe of record.

## TL;DR

```sh
make models          # regenerate all 29 rcp*.s3d into build/, verified against the backup
make install-models  # ...also deploy them to $GAME_DIR and patch GlobalLoad.txt + moddat.ini
```

Sources come from `MODEL_SRC_DIR` (default `~/Games/RoF2-modelswap-backup/model-inputs/`).
Print the recipe any time with `python3 tools/build_models.py --list`.

To hand the whole thing to a user as a drop-in folder instead of deploying locally, use
`make dist` (assembles `dist/` with the `.asi` + all 29 `.s3d` + `Resources/GlobalLoad.txt` +
`uifiles/rcp/**` + `INSTALL.txt`; see `tools/build_dist.py`).

## How the archives load (GlobalLoad.txt)

`Resources/GlobalLoad.txt` is the client's archive-load manifest (`format,persist,flags,name,tip`).
Stock RoF2 does not list any `rcp*` archive, so without the patch the client never loads the
classic meshes and model-swap silently renders stock. `tools/patch_globalload.py` inserts, into a
stock file:

- after the `gatesequip` equipment line: `rcpclassic` + `lgequip`/`lgequip2`/`lgequip_amr`/
  `lgequip_amr2`/`vequip` (the `lgequip*` archives un-gate the classic-helm/hair attach — see
  `src/npc_model_swap.cpp`), and
- after the `Global_chr` character line: `rcpglobal_chr`, `rcpwolf`, `rcptiger`, `rcpdrake`,
  `global5_chr`/`global5_chr2` (typed elementals), then the 24 per-race bodies (`TFFTC` =
  load-on-demand).

It is idempotent (a no-op once patched) and saves the stock file to `GlobalLoad.txt.rcpbak` before
the first edit. `--revert` removes the rcp lines; `--check` reports state.

## The recipe (every archive is one `isolate_archive.py` run)

`isolate_archive.py` copies a source `.s3d`, prefixes every WLD fragment label with `--prefix`
(namespace isolation, so classic meshes coexist with the modern pool without label collisions),
and applies same-length `--swap CODE=CODE` renames to the model code across labels, inline `0x03`
texture names, and PFS file entries. The mod DLL then redirects the modern actordef code to the
aliased classic one (`src/model_swap.cpp`, `src/npc_model_swap.cpp`).

| Output `.s3d` | Source archive | `--prefix` | `--swap` |
|---|---|---|---|
| `rcpclassic` | `gequip.s3d` (RoF2's own) | `RCP` | — |
| `rcpglobal_chr` | `global_chr.s3d` (TAKP classic) | `RCP` | `SKE=SK9,WOE=WO8,` then each race `M→9,F→8` |
| `rcpwolf` | `warrens_chr.s3d` (Trilogy) | `RC2` | `WOL=WO9` |
| `rcptiger` | `arena_chr.s3d` (Trilogy) | `RC3` | `TIG=TI9` |
| `rcpdrake` | `cobaltscar_chr.s3d` (Trilogy) | `RC4` | `DRK=DR9` |
| `rcp<race>7` (male, 12) | `global<race>m_chr.s3d` (TAKP) | `""` | `<RACE>M=<RACE>7` |
| `rcp<race>6` (female, 12) | `global<race>f_chr.s3d` (TAKP) | `""` | `<RACE>F=<RACE>6` |

Races (code, in GlobalLoad order): `HU BA ER EL HI DA HA DW TR OG HO GN`
(human, barbarian, erudite, wood-elf, high-elf, dark-elf, half-elf, dwarf, troll, ogre,
halfling, gnome). `rcpglobal_chr` full swap:

```
SKE=SK9,WOE=WO8,HUM=HU9,HUF=HU8,BAM=BA9,BAF=BA8,ERM=ER9,ERF=ER8,ELM=EL9,ELF=EL8,
HIM=HI9,HIF=HI8,DAM=DA9,DAF=DA8,HAM=HA9,HAF=HA8,DWM=DW9,DWF=DW8,TRM=TR9,TRF=TR8,
OGM=OG9,OGF=OG8,GNM=GN9,GNF=GN8,HOM=HO9,HOF=HO8
```

`build_models.py` encodes all of this; edit the recipe there, not by hand.

## Verification

`make models` verifies each rebuilt archive against a reference set (default the backup's
`built-s3d/`). Confirmed 2026-07-17: **25 / 29 regenerate byte-identical**; the 4 swap-heavy
specials (`rcpglobal_chr`, `rcpwolf`, `rcptiger`, `rcpdrake`) are **actordef-identical** (a few
bytes differ because their source archives were re-extracted; the models load identically).
`write_pfs` is deterministic, so byte-identity is the norm when the source bytes match.

## Source provenance (if `MODEL_SRC_DIR` is ever lost)

All build inputs are collected in the local backup (`model-inputs/`). To rebuild that dir from
scratch:

- **Classic `global*_chr.s3d`** (`global_chr` + 24 per-race `global<race><m|f>_chr`) — from the
  **TAKP "PC V2.1c"** client archive. Originally extracted with:
  `unzip -j "TAKP PC V2.1c.zip" "*global*_chr.s3d" -d <dir>`.
- **`warrens_chr.s3d`, `arena_chr.s3d`, `cobaltscar_chr.s3d`** — from an **EQ-Trilogy** client
  extract (`eqfiles/App_Executables/`).
- **`gequip.s3d`** — RoF2's own game dir (`$GAME_DIR/gequip.s3d`).

See the `asset-model-landscape` and `model-swap-progress` memories for where to obtain
other-version EQ files (archive.org Trilogy/Titanium/RoF2, TAKP, NostalgiaEQ). The `rcp*.s3d`
**outputs** are also kept verbatim in the backup's `built-s3d/` — a last-resort restore is just
copying those into `$GAME_DIR` + running `patch_globalload.py`.

## Restore-from-nothing checklist

1. Ensure `MODEL_SRC_DIR` exists (restore from backup, or re-extract per provenance above).
2. `make install` — deploy the `.asi` + uifiles.
3. `make install-models` — regenerate + deploy the 29 `.s3d` + patch `GlobalLoad.txt` and `moddat.ini`.
4. Relaunch the client. Verify with `/rcpmodels` and the Options **Models** tab (and that
   first-person camera height looks right on a classic troll/ogre — that's the `moddat.ini` fix).
5. Server-side (faction vision) is independent — see `CLAUDE.md` › server-side changes.
