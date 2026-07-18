# Classic (pre-revamp) zone maps

In-game map files for the RoF2 client's **pre-revamp `*_classic` zones**. The client loads
`maps/<zoneshortname>.txt` (plus optional `_1`/`_2`/`_3` overlay layers) when you enter a zone;
these files carry the *original* geometry's line-art for the three zones the client also ships
in classic form (`bazaar_classic.s3d`, `lavastorm_classic.s3d`, `nektulos_classic.s3d`).

The client's stock `maps/{bazaar,lavastorm,nektulos}.txt` are the **revamped** layouts and do
NOT match the classic geometry (different coordinate extents — verified: classic Bazaar is a
compact ~790×1379 vs the revamp's ~2900×2230; classic Lavastorm sits in an entirely different
coordinate space than the revamp). Hence a separate `_classic` set.

| File | Zone | Notes |
|---|---|---|
| `bazaar_classic.txt` | The Bazaar (classic) | POI labels baked into the base layer |
| `lavastorm_classic.txt` + `_1.txt` | Lavastorm Mountains (classic) | `_1` = zone-line / merchant labels |
| `nektulos_classic.txt` + `_1.txt` | Nektulos Forest (classic) | `_1` = zone-line / merchant labels |

## Provenance

Sourced from **Brewall's Maps** (https://www.eqmaps.info/), specifically the `_original`
(pre-revamp) variants Brewall keeps for revamped zones, via the vendored, PoP-era-filtered copy
in the sibling Zeal repo (`Zeal/zone_map_src/map_files/`, README there documents the 20240109
pull + `_original` selection). The classic Bazaar map was hand-verified classic in that pull.

The classic provenance was re-confirmed here against the client's own `*_classic.s3d` geometry:
the maps are byte-distinct from the stock revamp maps, and the Bazaar map's extent matches the
classic Bazaar mesh bounds (axis-swapped, the standard EQ map convention) while the revamp map
does not. The `_1` label layers name only classic zone connections (classic Lavastorm →
Solusek's Eye / Nagafen's Lair / Temple of Solusek Ro; classic Nektulos → East Commonlands).

Line endings normalized to CRLF to match every stock RoF2 `maps/*.txt`.

## Delivery

`make install` copies `maps/*.txt` into `$GAME_DIR/maps/`; `make dist` stages them under
`dist/maps/`. Unlike the model archives these are small and text, so they live in git.
