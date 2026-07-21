# Classic (pre-2013) spell icon sheets

The ten spell-icon texture sheets as they looked from 2001 until the Feb-2013
icon revamp replaced the art in-place (same filenames, same grid layout):

- `spells01.tga` … `spells07.tga` — 256x256, 40x40-cell grid (`A_SpellIcons`:
  spell book, buff windows, casting bar, item effect icons)
- `gemicons01.tga` … `gemicons03.tga` — 256x256, 24x24-cell grid
  (`A_SpellGems`: the spell-gem fills)

The mod's `/rcpspellicons` feature (`src/spell_icons.cpp`) loads these by path
at runtime and swaps them in for the client's revamped sheets **live** — the
stock files in `uifiles/default/` are never touched. `make install` copies this
folder to `$GAME_DIR/uifiles/rcp/spellicons/`.

## Provenance

Downloaded 2026-07-20 from the `.default/` folder (an untouched stock-skin dump
of the TAKP/Quarm client, which is PoP-era and so carries the original icon
art) of <https://github.com/draknarethorne/thorne-ui>, commit `0094e4d`.
Authenticity cross-checked against an independent client dump
(`KaelKodes/Everquest-Godot-Client`, `Assets/UI/ClassicUI/`): three sheets are
byte-identical, the rest differ only in the optional 26-byte TGA v2 footer.
This is the same art a pre-2013 client (e.g. Titanium) ships in
`uifiles/default/`.

Note when re-sourcing: avoid `thorne_drak/Options/Icons/Classic/` in that same
repo (machine-regenerated with gamma correction) and NillipussUI's sheets
(DuxaUI's custom redraws) — use a stock client dump.
