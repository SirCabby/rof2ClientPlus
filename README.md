# rof2ClientPlus

A minimal client-side mod for the EverQuest **Rain of Fear 2** client, in the
spirit of [Zeal](https://github.com/CoastalRedwood/Zeal).

## How it loads (the ASI vector)

`eqgame.exe` imports `mss32.dll` (Miles Sound System). When sound initializes,
Miles calls `LoadLibrary` on **every `*.asi` file in the game root**. So a normal
32-bit DLL renamed `rof2ClientPlus.asi` and dropped in the game folder gets
loaded, and its `DllMain` runs — no injector needed. (Sound must be enabled:
`Sound=TRUE` in `eqclient.ini`.) On load we **pin** the module so it is never
unloaded, then a worker thread installs the render hook.

The `EndScene` hook is installed by creating a throwaway D3D9 device, reading the
(shared) `IDirect3DDevice9` vtable, and swapping the `EndScene` slot. Every D3D9
device from the same `d3d9.dll` shares one vtable, so the game's device then
calls ours. The hook paints a small red bar top-left each frame and chains to the
original.

## Build (Linux, mingw-w64)

No MSVC or Windows required — cross-compiled 32-bit:

```sh
cp config.mk.example config.mk    # then edit config.mk: set GAME_DIR to your RoF2 folder
make                              # -> build/rof2ClientPlus.asi
make install                      # copies it into $GAME_DIR
```

`config.mk` is gitignored, so your game path stays out of the repo. `make`
(build only) works without it; `make install` needs `GAME_DIR` — set it in
`config.mk`, or pass a one-off override: `make install GAME_DIR=/path/to/RoF2`.

## Run & verify

1. `make install`
2. Launch the client (`~/Games/RoF2/launch-eq.sh`).
3. Watch the log: `tail -f ~/Games/RoF2/rof2ClientPlus.log`
   - a `DllMain PROCESS_ATTACH` line on load,
   - `render hook installed = true`,
   - `EndScene hook fired, frame N` lines while in game.

Uninstall: delete `rof2ClientPlus.asi` from the game folder.

## Layout

| File | Role |
|------|------|
| `src/dllmain.cpp` | entry point: pin module, spawn worker, message box |
| `src/directx.*`   | D3D9 device discovery + `EndScene` hook + overlay |
| `src/hooks.*`     | vtable-swap helper (inline detours can be added later) |
| `src/logger.*`    | tiny file logger written next to `eqgame.exe` |

Structure mirrors Zeal (`dllmain` / `hooks` / `directx`) so features slot in.
