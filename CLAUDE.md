# rof2ClientPlus — project guide

A client-side ASI mod for the EverQuest **Rain of Fear 2** client (32-bit), cross-built
on Linux with mingw-w64. Features and RE details live in `README.md` and, in depth, in
`PORTING_NOTES.md`. This file is the map of **what the mod changes and how each change is
delivered** — read the next section before assuming `make install` ships everything.

## Build & deploy

```sh
make                 # -> build/rof2ClientPlus.asi   (config.mk sets GAME_DIR; gitignored)
make install         # deploy .asi + option-window overrides (->uifiles/default) + fonts/rings (->uifiles/rcp) + eq-window-fix
make install-models  # deploy the 29 classic-model .s3d + patch Resources/GlobalLoad.txt
make dist            # assemble dist/ (gitignored): a drop-in bundle to hand to a user
```

A **full/fresh deploy is `make install && make install-models`.** `make install` alone does
NOT deliver the model-swap feature's runtime data (see below).

**`make dist`** builds a self-contained `dist/` folder mirroring the client layout (the `.asi`,
the 29 `rcp*.s3d`, `Resources/GlobalLoad.txt` = stock + the mod's lines, `uifiles/**` — option
windows in `default/`, fonts/rings in `rcp/` — and an `INSTALL.txt`). Copying its contents into a
RoF2 dir is a complete install — every file lands where it belongs. This is the artifact to distribute; it does not require the recipient to have the build
toolchain or the model sources. (`tools/build_dist.py`; the vendored stock manifest is
`tools/GlobalLoad.stock.txt`.)

## ⚠️ Out-of-band artifacts — changes that live OUTSIDE `make install`

`make install` copies: `rof2ClientPlus.asi`; the option-window overrides
`uifiles/default/*.xml` (into the client's **default skin** — see UI note below);
`uifiles/rcp/fonts/*.spritefont`, `uifiles/rcp/targetrings/*.tga`; and `eq-window-fix`.
Everything else the mod touches is delivered by a **separate** mechanism. Do not assume a
clean `git clone` + `make install` reconstitutes a working setup — it does not include the
model-swap assets or the server-side changes. The complete map:

| Artifact | Where it lives | In git? | Delivered by | Reproduce / restore |
|---|---|---|---|---|
| The `.asi` + fonts/target-rings | repo `src/`, `uifiles/rcp/{fonts,targetrings}` | ✅ yes | `make install` | `make && make install` |
| **Option-window overrides** (`EQUI_OptionsWindow.xml` carrying `/rcpoptions` + `EQUI_AdvancedDisplayOptionsWnd.xml`) | repo `uifiles/default/` → client `uifiles/default/` (**overwrites 2 stock files**) | ✅ yes | `make install` (backs stock up ONCE as `.rcpbak`) | regenerate: `gen_option_overrides.py` (from vendored `tools/stock-uifiles/`) + `gen_rcp_options_ui.py` |
| **29 `rcp*.s3d` classic-model archives** (59 MB) — model-swap | `$GAME_DIR/*.s3d` only | ❌ no | `make install-models` | `make models` regenerates all 29 from source (see below) |
| **29 `rcp*.s3d` classic-model archives** (59 MB) — model-swap | `$GAME_DIR/*.s3d` only | ❌ no | `make install-models` | `make models` regenerates all 29 from source (see below) |
| **`$GAME_DIR/Resources/GlobalLoad.txt`** edit (loads the rcp archives) | game dir only | ❌ no | `make install-models` | `tools/patch_globalload.py` (idempotent; stock saved as `.rcpbak`) |
| **`$GAME_DIR/Resources/moddat.ini`** edit (first-person camera offsets for the 23 alias models) | game dir only | ❌ no | `make install-models` | `tools/patch_moddat.py` (idempotent; vendored `tools/moddat.stock.ini` + `.rcpbak`) |
| **Server-side faction-vision** rule + impl + migration | `~/workspace/GitHub/akk-stack` (see below) | ✅ (other repos) | rebuild EQEmu + run migration | branches pushed to SirCabby GitHub forks |
| `rof2ClientPlus.ini` (tuned settings) | game dir | ❌ (per-machine) | written by the mod at runtime | backed up in the local backup; regenerated with defaults if absent |

### UI overrides & skins (skin-independent as of 2026-07-17)

The mod's option windows live in the client's **`uifiles/default/`** (base skin), so `/rcpoptions`
and the widened option text load **under any UI skin** — users can run `default` or their own custom
skin freely. `uifiles/rcp/` is now only the mod's **asset** folder (fonts + target-rings, read by the
DLL by path), NOT a skin to select; `UISkin=rcp` is obsolete.

**Delivery:** the `/rcpoptions` window is a **standalone `EQUI_RcpOptions.xml`** shipped into
`default/`, pulled in by a one-line `<Include>` added to a copy of stock `EQUI.xml` (also in
`default/`) — the standard way any skin adds a window, parsed by the client's native UI load.
`EQUI_OptionsWindow.xml`/`EQUI_AdvancedDisplayOptionsWnd.xml` are separate widened-text overrides.
`make install` overwrites 3 stock files (`EQUI.xml` + the 2 option windows), saving each pristine
copy **once** as `*.rcpbak` (never re-backs-up over its own file; `EQUI_RcpOptions.xml` is a pure mod
file, marked so it is never backed up), and removes the old `uifiles/rcp/*.xml`.

**Lessons this cost a session:**
- A standalone included window does **NOT** crash the parse — the old "standalone crashes" was the
  *runtime* `WriteTemporaryUI` include-merge; a **static** on-disk `EQUI.xml` `<Include>` works (129
  controls, confirmed in-game). The `XMLRead`/`WriteTemporaryUI` redirect is gone.
- `/loadskin` rebuilds the UI **in-game**, freeing our window; `ui_manager.cpp`'s `LoadSidl` hook
  drops `RcpOptionsUI`'s cached handles on any `EQUI.xml` (re)load so the frame poll can't touch a
  freed control. That hook must **NOT** use `rcp->callbacks` — `CallbackManager` is not instantiated
  in this build (null; using it crashed char-select). See `crash-window-diagnostics`.
- Edge case: a *full* custom skin shipping its **own** `EQUI.xml` wouldn't have our `<Include>`, so
  `/rcpoptions` wouldn't load under it — rare (most skins are partial and inherit `default/EQUI.xml`).

See the `ui-override-stock-windows` memory.

### Model-swap assets (the one place work could be permanently lost)

The 24 per-race + 5 special `rcp*.s3d` were each built by one `tools/isolate_archive.py`
run. That recipe is now recorded and automated:

- **`tools/build_models.py`** — batch driver; the single source of truth for the recipe
  (`--list` prints it). Regenerates all 29 from source archives and verifies each against a
  reference (25 rebuild **byte-identical**, 4 swap-heavy ones actordef-identical). Run via
  `make models`. `--install` deploys + patches GlobalLoad.txt AND moddat.ini (this is `make install-models`).
- **`tools/patch_globalload.py`** — idempotently adds the rcp archive lines to
  `Resources/GlobalLoad.txt` (the client's archive-load manifest; the mod DLL never writes it).
- **`tools/patch_moddat.py`** — idempotently appends per-alias-code camera offsets (`FPCOffset`/
  `ZPCOffset`) to `Resources/moddat.ini` so first-person camera height is right on classic PC-race
  swaps (worst on troll/ogre). Vendored stock: `tools/moddat.stock.ini`.
- **Sources** (classic `global*_chr.s3d` from TAKP, `warrens/arena/cobaltscar_chr.s3d` from
  EQ-Trilogy, RoF2's own `gequip.s3d`) are collected in `MODEL_SRC_DIR`
  (default: the local backup's `model-inputs/`). If lost, they are re-obtainable — see
  `docs/MODEL_ASSETS.md` for exact provenance and the full recipe.

### Local backup (safety net)

`~/Games/RoF2-modelswap-backup/` holds the working bytes so nothing is lost even if the game
dir is wiped: `built-s3d/` (the 29 archives), `model-inputs/` (all 29 build sources),
`globalload/` (patched `GlobalLoad.txt` + stock `.rcpbak`), `settings/` (`rof2ClientPlus.ini`).
See its `README.md`. This is a convenience net; the durable reproduce path is `make models`.

### Server-side changes (a separate stack, backed up on GitHub)

Faction-vision (kill/consider standing in chat) is **server-side**, in `~/workspace/GitHub/akk-stack`:
- `akk-stack` → `origin` = SirCabby/akk-stack, branch `cabby-enhanced`
- `akk-stack/code` (EQEmu fork) → SirCabby/EQEmu, branch `cabby-enhanced` — rule `Client:RcpFactionVision` + `client.cpp`/`client_packet.cpp`
- `akk-stack/eqemu-ops` → SirCabby/eqemu-ops, `master` — the enable migration

All three track their `origin` and are pushed (safe). Rebuild: `cmake --build ~/code/build` +
`docker restart`; apply DB via dbmate. See the `faction-vision-implementation` memory.

## Layout & conventions

- `src/*.cpp` — one file per feature; `src/rcp.*` wires them into the `RcpService` singleton.
- Offsets/structs are for the **2013-05-10 RoF2 build**; the vendored `eqlib` offsets header
  (`~/workspace/GitHub/eqlib/include/eqlib/offsets/eqgame.h`) is named offsets for this exact
  build — grep it before any RE.
- `tools/` — Python asset tooling (all pure-stdlib): model build (`build_models.py`,
  `isolate_archive.py`, `make_classic_alias.py`), `patch_globalload.py` + `patch_moddat.py`
  (+ vendored stock `GlobalLoad.stock.txt` / `moddat.stock.ini`), `build_dist.py` (the drop-in
  packager), UI generation
  (`gen_rcp_options_ui.py`, `gen_option_overrides.py`), analysis (`model_inventory.py`,
  `validate_models.py`).
- The user commits every repo himself — do not `git commit`/`push` unless asked.
- Deploy after a successful build (`make install`); install is atomic (temp + `mv`) because a
  running client mmaps the `.asi`. See the `deploy-preference` memory.
