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
  `/rcpopts`). Settings persist to `rof2ClientPlus.ini` next to `eqgame.exe`.
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
UI is delivered by hooking the client's `EQUI.xml` loader to merge in
`uifiles/rcp/EQUI_RcpOptions.xml`, which is shown in sync with the stock Options
window.

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
`make install` also copies `uifiles/rcp/` into `$GAME_DIR/uifiles/rcp/` (the
options window needs these).

### Windows (Visual Studio 2022)

Open `rof2ClientPlus.sln` and build **Release / Win32 (x86)** →
`build-msvc\Release\rof2ClientPlus.asi`. Needs only the Windows SDK. The CRT is
statically linked (`/MT`), so the `.asi` needs no VC++ redistributable. Copy the
built `.asi` into your RoF2 folder and copy `uifiles/rcp/` into
`<RoF2>/uifiles/rcp/` (the `config.mk` / `make install` flow is Linux-only).

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

Uninstall: delete `rof2ClientPlus.asi` and `uifiles/rcp/` from the game folder.

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
| `uifiles/rcp/` | `EQUI_RcpOptions.xml` + `EQUI_Tab_Cam.xml` (the options UI) |

Structure mirrors Zeal so features slot in. See [`NOTICE`](NOTICE) for
vendoring/attribution.
