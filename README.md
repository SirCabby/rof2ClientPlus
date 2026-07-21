# rof2ClientPlus

A client-side mod for the EverQuest **Rain of Fear 2** client, in the spirit of
(and vendoring substantial code from) [Zeal](https://github.com/CoastalRedwood/Zeal).

## Features

- **`/rcpcam`** (alias `/rcpsmoothing`) — smooth mouse-look and a Zeal-style
  third-person chase camera (scroll out of first person to a smooth,
  collision-aware chase cam; adjustable zoom, FOV, chase/autofollow). This is a
  faithful port of Zeal's `/zealcam`.
  - `/rcpcam` toggles smoothing; `/rcpcam x y` or `/rcpcam x y x3 y3` sets
    first/third-person sensitivities; `/rcpcam info` prints them.
  - Supporting commands: `/fov`, `/pandelay` (`/pd`), `/selfclickthru`,
    `/playerclickthru`, `/leftclickcon`, `/dampenlev`.
- An **options window** with a **Cam** tab (Enabled, sensitivity sliders, FOV,
  pan delay, view-cycle toggles), opened/closed with **`/rcpoptions`** (alias
  `/rcpo`). Settings persist to `rof2ClientPlus.ini` next to `eqgame.exe`.
- **`/rcpequip`** — right-click a wearable item in a bag to auto-equip it into
  the best slot. A **clicky** item casts instead (as it does natively); use
  **Alt+right-click** to equip a clicky. Ships off; toggle with `/rcpequip
  on|off` or the **Right-click to equip** checkbox on the `/rcpoptions` Mouse
  tab. A RoF2-native port of Zeal's `equip_item`.
- **`/rcpspellicons`** (alias `/rcpicons`) — swap the spell book / gem / buff
  icons between the **original pre-2013 art** and the revamped client default,
  **live** (no relog, no `/loadskin`, no file swapping — the swap happens on
  the loaded textures in memory). The classic sheets ship in
  `uifiles/rcp/spellicons/` (see its README for provenance); also a **Classic
  spell icons** checkbox on the `/rcpoptions` Display tab.
- `/rcp` lists all commands. `/uierrors on|off` toggles UI skin error reporting.

> **Attribution:** large portions of this project are adapted from Zeal
> (MIT-licensed, © 2024 Clint). See [`NOTICE`](NOTICE). The code has been
> rebranded to `rcp` (namespace, symbols, commands, ini, and uifiles); Zeal is
> credited as the original source, not our identity.

## How it loads (the ASI vector)

`eqgame.exe` imports `mss32.dll` (Miles Sound System). When sound initializes,
Miles calls `LoadLibrary` on **every `*.asi` file in the game root**. So a normal
32-bit DLL renamed `rof2ClientPlus.asi` and dropped in the game folder gets
loaded, and its `DllMain` runs — no injector needed. (Sound must be enabled:
`Sound=TRUE` in `eqclient.ini`.) On load we **pin** the module so it is never
unloaded, then a worker thread installs the render hook.

On load, the worker thread creates the `RcpService` singleton, whose constructor
installs the mod's features: **inline detour hooks** on the client's command
interpreter, mouse/camera routines, and UI (`EQUI.xml`) loader — all at fixed
`eqgame.exe` addresses. A separate `EndScene` hook (throwaway-D3D9-device vtable
swap) remains as a harmless frame-counter proof-of-life; it no longer draws the
old red test bar.

The camera feature hooks the client's mouse-look and `DoCamAI` routines to add
mouse-delta smoothing and a replacement third-person chase camera. The options
window (`/rcpoptions`) is delivered by shipping an override of the stock
`EQUI_OptionsWindow.xml` — with the mod's option UI defined inside it — into the
client's `uifiles/default/` (base skin), so it loads under **any** UI skin. (The
pristine stock file is saved once as `EQUI_OptionsWindow.xml.rcpbak`.)

## Build

The same 32-bit source builds two ways and produces a behavior-identical
`rof2ClientPlus.asi` either way. End users don't build at all — they just drop a
released `.asi` into the game folder.

### Linux (mingw-w64)

Cross-compiled 32-bit; no MSVC or Windows required:

```sh
cp config.mk.example config.mk    # then edit config.mk: set GAME_DIR to your RoF2 folder
make                              # -> build/rof2ClientPlus.asi
make install                      # copies it into $GAME_DIR
```

`config.mk` is gitignored, so your game path stays out of the repo. `make`
(build only) works without it; `make install` needs `GAME_DIR` — set it in
`config.mk`, or pass a one-off override: `make install GAME_DIR=/path/to/RoF2`.
`make install` also deploys the option-window overrides into
`$GAME_DIR/uifiles/default/` (the base skin — backing up the stock files once as
`*.rcpbak`) and the mod's fonts/target-rings into `$GAME_DIR/uifiles/rcp/`.

The **model-swap** feature also needs 29 classic-model `.s3d` archives plus a
`Resources/GlobalLoad.txt` edit, which `make install` does *not* ship —
`make install-models` deploys those (see `docs/MODEL_ASSETS.md`). A full local
deploy is `make install && make install-models`.

### Distributable bundle (`make dist`)

`make dist` assembles a `dist/` folder that mirrors the client layout — the `.asi`,
all 29 `rcp*.s3d`, `Resources/GlobalLoad.txt`, `uifiles/**` (option windows in `default/`,
fonts/rings in `rcp/`), and an `INSTALL.txt`. Hand the folder to a user; copying its
contents into their RoF2 directory is a complete, drop-in install (no build toolchain
required on their end).

### Windows (Visual Studio 2022)

Open `rof2ClientPlus.sln` and build **Release / Win32 (x86)** →
`build-msvc\Release\rof2ClientPlus.asi`. Needs only the Windows SDK. The CRT is
statically linked (`/MT`), so the `.asi` needs no VC++ redistributable. Copy the
built `.asi` into your RoF2 folder, copy `uifiles/default/*.xml` into
`<RoF2>/uifiles/default/` (back up the two stock files first), and copy
`uifiles/rcp/` into `<RoF2>/uifiles/rcp/` (the `config.mk` / `make install` flow is
Linux-only). Simplest: use the `make dist` bundle instead.

## Run & verify

1. `make install`
2. Launch the client (`~/Games/RoF2/launch-eq.sh`).
3. Watch the log: `tail -f ~/Games/RoF2/rof2ClientPlus.log`
   - a `DllMain PROCESS_ATTACH` line on load,
   - `RcpService created` and `render hook installed = true`,
   - `EndScene hook fired, frame N` lines while in game.
4. In game (no red bar): scroll out of first person for a smooth chase cam;
   right-mouse look is smoothed. Run `/rcpcam info`. Run `/rcpoptions` → the
   **Cam** tab adjusts the same settings. Relog and confirm the settings
   persisted in `rof2ClientPlus.ini`.

Uninstall: delete `rof2ClientPlus.asi` and `uifiles/rcp/`, and restore the two stock
option windows from `uifiles/default/*.rcpbak` (rename them back over the mod's copies).
For the model-swap assets too, also delete the `rcp*.s3d` and run
`tools/patch_globalload.py --revert`.

## Layout

| File / group | Role |
|------|------|
| `src/dllmain.cpp` | entry point: pin module, spawn worker, create `RcpService` |
| `src/rcp.*` | the service singleton that owns and wires up all modules |
| `src/camera_mods.*` | the `/rcpcam` camera + mouse-handling feature |
| `src/ui_manager.* · ui_options.* · ui_skin.*` | options window + uifile loading |
| `src/commands.* · callbacks.* · binds.* · rcp_settings.*` | command/callback/settings framework |
| `src/hook_wrapper.* · instruction_length.h · memory.*` | inline detour hooking engine |
| `src/game_*.h · game_functions.* · camera_math.* · vectors.*` | reverse-engineered client interface (`Rcp::`, adapted from Zeal) |
| `src/directx.* · hooks.* · logger.*` | baseline D3D9 hook, vtable helper, file logger |
| `uifiles/default/` | override copies of stock `EQUI_OptionsWindow.xml` (with the `/rcpoptions` UI defined inside it) + `EQUI_AdvancedDisplayOptionsWnd.xml`, clipped text widened. Deploy into the client's base skin so the mod UI loads under any skin. Regenerate: `tools/gen_option_overrides.py` (from vendored `tools/stock-uifiles/`) then `tools/gen_rcp_options_ui.py` |
| `uifiles/rcp/` | mod assets loaded by the DLL by path: `fonts/*.spritefont` (nameplates) + `targetrings/*.tga` (target ring). Not a UI skin |

Structure mirrors Zeal so features slot in. See [`NOTICE`](NOTICE) for
vendoring/attribution.
