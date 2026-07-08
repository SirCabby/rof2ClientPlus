# rof2ClientPlus — client-interface porting notes (TAKP → stock RoF2)

Working notes for repointing the vendored Zeal (TAKP) interface to the stock
**Rain of Fear 2** client. Zeal is MIT; addresses here are facts about the same
`eqgame.exe` we target, cross-checked against the locally-cloned **eqlib**
(MacroQuest, GPL — used as a fact-reference only; this project stays MIT).

## Target binary
- `/home/joshua/Games/RoF2/eqgame.exe` — build **May 10 2013**, md5 `e9d416c6de6b53008ae00c2b16bbfd6c`.
- eqlib fact-reference: `/home/joshua/workspace/GitHub/eqlib` (offsets in
  `include/eqlib/offsets/eqgame.h`, structs in `include/eqlib/game/*.h`).

## Regenerating the disassembly (scratchpad is session-specific — it will be gone)
```
objdump -d eqgame.exe > eqgame.asm      # AT&T syntax, what the notes below quote
```
Reading raw floats/vtables straight from the PE in python:
- `.rdata`: file offset = `VA - 0x9c0000 + 0x5be800`
- Confirmed: PE image base makes `.text` run ~`0x401000..0x9c0000`.

## Build / install / test loop
- `make && make install` → drops `rof2ClientPlus.asi` (+ `uifiles/rcp/`) into `/home/joshua/Games/RoF2/`.
- Runtime log: `/home/joshua/Games/RoF2/rof2ClientPlus.log`. Wine log: `eq-last-run.log`.
- The user launches via `/home/joshua/Games/RoF2/launch-eq.sh` (direct `wine eqgame.exe patchme`).

---

## Phase 1 — first-person mouse-look — DONE (confirmed by user)
Lives in `src/mouse_mods.cpp` (the `/rcpcam` feature). All addresses are in the
file's comments. Summary of the mechanism + the RE behind it:

**Root cause of the old stair-stepping:** the stock turn function `0x516d40`
loads the mouse delta with `fildl` (INTEGER→float), so a slow axis's sparse
whole counts (`0,0,0,1`) become visible pitch jumps. Rewriting the integer
DInput buffer can't fix it (no sub-count precision).

**Fix:** hook `IDirectInputDevice8::GetDeviceState` (mouse device vtable at
`0xE67B50`, slot 9), and while looking (`MouseLook 0xDDF702` && right btn
`0xDDF73D`) reproduce the client's turn math in FLOAT on a low-pass-smoothed
delta, applied directly to the controlled entity, then zero the buffer
(`0xE67884` lX / +4 lY) so the client's own integer turn is suppressed.

**Client turn calibration (extracted from `0x516d40`):**
- `S = (mouseSensIni - 1) * 0.1428571 * 1.5 + 0.5`  (mouseSensIni @ `0xDDF69C`)
- yaw   `= (dx / screen_width)  * 512 * S`
- pitch `= (dy / screen_height) * 256 * S`
- screen: left `0xDDF620`, right `0xDDF628`, top `0xDDF624`, bottom `0xDDF62C`
- Applied as: `Heading -= dyaw` (wrap 0..512), `Pitch -= dpitch` (clamp ±128).
  Note pitch sign is **negated** (mouse-up looks up). Yaw written by the client
  as a *rate* to SpeedHeading — we zero SpeedHeading@0x8c to cancel it.

**Controlled entity + offsets (eqlib PlayerClient, cross-checked in disasm):**
- controlled player ptr `*(void**)0xDD2644` (self/mount/charm; local player is `0xDD2630`)
- `Heading @ 0x80`, `SpeedHeading @ 0x8c`, `CameraAngle(pitch) @ 0x90`
- Gated to first person only via `cameraType == 0` (`0xD1FD9C`); other camera
  modes fall through to the client's native turn (Phase 2 territory).

---

## Phase 2 — third-person chase camera — 2b (positioning) IMPLEMENTED, awaiting in-game test
Lives in `src/chase_cam.cpp` (the `/rcpchase` feature), a **self-contained** module
in the mouse_mods.cpp style. The vendored `src/camera_mods.cpp` is a TAKP reference
only and **cannot be address-swapped**: RoF2's camera model is fundamentally
different (see below), so it stays unconstructed.

### Key architecture finding — RoF2 ≠ TAKP camera model
- Stock RoF2 uses **polymorphic `EQCamera` objects** (each with a vtable) in a
  pointer array; TAKP used one flat `CameraInfo` struct array. So there is **no
  single `DoCamAI`** to hook — positioning is a per-camera **vtable method**.
- **The vendored `camera_mods.cpp` `DoCamAI 0x4db384` is a switch JUMP TABLE in
  RoF2, not a function — hooking it would crash.** (Confirmed.)
- **The mouse-wheel chase camera is `cameraType 6` (`Cameras[6]@0xDE0D7C`), NOT 2.**
  The vendored enum `RcpCam=2` is TAKP. In RoF2, F9's `CHASE_CAMERA`=2 is a
  *separate* camera from the wheel-out follow camera (=6). ZealCam replaces the
  wheel one, so we target **6**.

### Confirmed stock-RoF2 addresses (all disasm-verified)
- **cameraType (int) `0xD1FD9C`**: 0=first person, 6=wheel chase. CDisplay ptr `0xDD2660`.
- **Cameras array `0xDE0D64`** (`EQCamera*[8+]`): active = `*(EQCamera**)(0xDE0D64+cameraType*4)`.
  Slots→vtables (from init `0x5338A0`): [0]fp `0x9d0f80`, [1]overhead `0x9d10c8`,
  [2]F9-chase `0x9d1108`, [3/4]user `0x9d1148`, [5] `0x9d11c8`, **[6]wheel-follow `0x9d1188`**.
- **EQCamera layout** (eqlib `EQData.h`): `Y@0x04 X@0x08 Z@0x0c`,
  `Orientation_Y@0x10 Orientation_X@0x14 Orientation_Z@0x18` (**the renderer reads
  position 0x04/0x08/0x0c + orientation 0x10/0x14/0x18**), `Heading@0x28 Height@0x2c
  Pitch@0x30 Distance@0x34 DirectionalHeading@0x38 Zoom@0x40 bAutoPitch@0x44 bAutoHeading@0x45`.
- **Per-frame camera driver `0x796DD0`** (called from `RealRender_World 0x49CA10`
  at `0x49CE5B`): calls `Cameras[cameraType]->vtable[0x08](viewActor)` then reads
  the camera's position+orientation into the renderer. Do NOT tail-hook this (too late).
- **Cam6 positioner = `0x799140`** (vtable `0x9d1188` slot +0x08), `__thiscall(EQCamera*
  ecx, PlayerClient* actor)`, `ret $4`, first 5 bytes `83 ec 30 56 57` = clean detour.
  Writes Position (via `0x490f60`) + Orientation. Native uses smoothed
  `DirectionalHeading@0x38` for orbit position, raw entity `Heading@0x80` for look.
  Orientation_X native = `Pitch@0x30 − 8.5` (`0xa002e0`). **← our tail-hook target.**
- **Mouse-wheel game handler `0x518A70`** (`__thiscall`, this=`*(void**)0xE67CCC`,
  arg=wheelDelta; called from `ProcessMouseEvents 0x539E60` at `0x53A186`). Native
  path toggles cameraType `0↔6` via **`SetCameraType 0x48ADF0`** (`__stdcall(int)`,
  writes `0xD1FD9C`), zoom accumulator `CEverQuest+0x5EC` clamped to
  `[min, gfMaxZoomCameraDistance 0x9D0DE4 = 53.0]`. (Note: eqlib's `0x5F9C90` is only
  the low-level DInput wheel accumulator → `0xE67B64`, NOT the game handler.)
- **PlayerClient offsets** (controlled `*(PlayerClient**)0xDD2644`, self `0xDD2630`;
  eqlib + disasm): pos `Y@0x64 X@0x68 Z@0x6c` (feet), `Heading@0x80` (0..512),
  `CameraAngle/pitch@0x90`, `SpeedHeading@0x8c`, `AvatarHeight@0x138` (head height),
  `Height@0x13c`, `Type@0x125` (Player=1), `Race@0xeb4`, `SpawnID@0x148`,
  `Mount@0x154`, `WhoFollowing(autofollow)@0xe40`, viewactor `mActorClient@0xea4 +
  pActor@0x101c` (`CActorInterface*`, all-virtual; BoundingRadius vtable +0x60/+0x64).
- Phase-3 refs: `SetViewActor 0x48F030` (bad-cam msg patch: `0x48F091` `74 27`→`EB 27`),
  `GetClickedActor 0x48B6B0`, exact eye-height virtual `0x0058CF00` (thiscall).

### Still to map (later increments)
- `collide_with_world` raycast (camera collision, 2c).
- `get_region_from_pos` (water/region tint for the camera).
- Independent-yaw mouse input in chase mode (the smoothed delta — reuse Phase 1's
  DInput hook path; mouse_mods only acts in cameraType 0 today).

**Status — 2b WORKING (user-confirmed), plus mouse-sensitivity + options UI fixed:**
1. **2b chase positioning (`src/chase_cam.cpp`, WORKING)** — tail-detour `0x799140`;
   while enabled && `cameraType==6`, reuse the client's OWN computed camera-behind
   vector `(camPos−playerPos)` (correct direction, honors left-click orbit, keeps
   player centered) and only **rescale distance** along it + optional **Z height
   raise**; orientation left native. Off by default; `/rcpchase on|off|dist
   <n|native>|height <n>`, ini `[Chase]`.
   - *Earlier v1 (superseded) recomputed the orbit with `get_cam_pos_behind` +
     forced look=Heading → camera went in-front and player off-centre. Lesson:
     don't recompute the orbit; reuse the client's vector.*
2. **Mouse sensitivity in third person (`src/mouse_mods.cpp`, WORKING)** — the
   `/rcpcam` float-turn now engages in `cameraType==0` AND `cameraType==6` (was
   first-person only). In chase view it drives heading (turn) + suppresses the
   native horizontal turn, leaving `lY`/pitch to the native camera.
3. **Options window (`src/rcp_options_ui.cpp` + `uifiles/rcp/*.xml`, WORKING)** —
   sliders/checkbox now live. Three bugs were fixed: (a) **service creation race** —
   `ProcessGameEvents_hk` could build two `RcpService` instances, so `on_frame`
   polled a different (empty) options object than `/rcpoptions` built; fixed with a
   **thread-safe magic-static one-time create** in `dllmain.cpp`. (b) controls were
   in a dynamically-created **TabBox/Page** that renders but doesn't route mouse
   input → flattened to **direct Screen children**. (c) `on_frame` gated on a stale
   `is_visible` → dropped that gate (poll whenever the handle exists). Slider
   internals (disasm-verified): value@0x218, max@0x220, thumb-scale@0x21c,
   thumb-off@0x224; `GetValue 0x895FE0`, `SetValue 0x8961B0`.

**2c world-collision pull-in (IMPLEMENTED in `chase_cam.cpp`, needs in-game test)** —
`/rcpchase collision on|off` (off by default). In the positioner, after computing
the wanted camera pos, cast pivot(playerXY@camZ)→wanted through world geometry and
pull the camera to the clamp point (×0.9 margin) on a hit. Uses the client's own LOS
primitive (all disasm-verified, coords are EQ-native **Y,X,Z**):
- `pCollision = *(void**)0x15D46B0` (SGraphicsEngine+0x14; vtable in eqgraphics.dll
  base ≥0x10000000 — guard on that before calling).
- Build info: `CCollisionInfoTargetVisibility` ctor **`0x8D4570`** `__thiscall(info,
  seg[6]={from.Y,from.X,from.Z,dY,dX,dZ}, excludeEntity=*(void**)0xDD2644, 0)`,
  ret $0xc; `info` = zeroed ≥0xA0-byte buffer.
- Test: `blocked = pCollision->vtable[0](pCollision, info)` (nonzero = blocked; or
  read `info+0x58`).
- Clamp point: **`0x7A2680`** `__thiscall(info, out[3])` → `out = start + t·dir`
  (Y,X,Z); `t` inits to 1.0 so out==wanted when clear.
- Ground-clamp fallback `CDisplay::GetFloorHeight 0x48C5B0`. Region (secondary):
  `pSceneGraph=*(void**)0x15D46A8`, `GetRegionNumber` = scenegraph vtable byte 0x48.

**Still to do (rest of 2c):** independent smooth camera yaw/pitch (mouse) — note LMB
already orbits + RMB turns via the native path under 2b — plus head-height
look-at-target (native `Orientation_X = Pitch@0x30 − 8.5`), zoom-interp smoothing,
and add the chase settings to the (now-working) options window.

**Phase 3 (later):** `/fov` (SetCameraLens hook in `eqgfx_dx8.dll`
`t3dSetCameraLens`), self/player click-through (GetClickedActor `0x48B6B0`
bounding-radius minimize), `/leftclickcon`, `/dampenlev`, `/pandelay`,
toggle-cam binds; wire the options window to the camera settings.

---

## Nameplate port (Zeal `nameplate.cpp` → RoF2) — N1 tint CONFIRMED, N2 text IMPLEMENTED
Lives in `src/nameplate.cpp` (the `/rcpnameplate` feature), a self-contained module in the
chase_cam.cpp style. Reference: `/home/joshua/workspace/GitHub/Zeal/Zeal/nameplate.cpp` (TAKP).

### Key architecture finding — RoF2 ≠ TAKP nameplate model
- In TAKP the nameplate ops were **`CDisplay` methods** taking an entity arg, and Zeal poked
  the color into a `StringSprite->Color` **data field**. In RoF2 they are **`PlayerClient`
  methods** (`this` == the entity) and color/text are applied through the entity's
  **actor-interface vtable**, not a data field.
- eqlib names them `PlayerClient::SetNameSpriteState/Tint`; addresses already match this build.

### Confirmed stock-RoF2 addresses (eqlib + disasm-verified)
- **`SetNameSpriteTint()` `0x58BF00`** — `__thiscall(this)`, no args, returns int (1 = tint
  applied). Guards `actor != 0` at entry; ends by `actor->vtable[+0x190](&color)` where
  `actor = *(void**)(entity+0x101c)` and `&color` points at the color bytes the client builds
  on its stack at `[esp+0xc..0xf]`. **Byte order is D3DCOLOR little-endian `B, G, R, A`**
  (`[+0xc]`=B, `[+0xd]`=G, `[+0xe]`=R, `[+0xf]`=A) — **confirmed in-game** (an initial R,G,B
  guess rendered a red con name as blue). So `apply_tint()` writes B,G,R,A.
- **`SetNameSpriteState(bool show)` `0x58E2D0`** (N2) — `__thiscall(this, show)`, `ret 0x4`.
  Sets text via `actor->vtable[+0x18c](0, 0, textPtr)`; `+0x1a4` = is-name-shown(bool).
  `bDisplayNameSprite @ entity+0x1228` gates it; race-hide uses `Animation @ entity+0x10c0`
  (values 0x10/0x21/0x26) and virtual `GetRace` (`[this]+0x88`).
- **Actor-interface `*(void**)(entity+0x101c)` vtable**: `+0x18c` set-string, `+0x190`
  set-tint(colorPtr), `+0x1a4` is-name-shown. Slot index = offset/4.

### RoF2 PlayerClient (Entity) offsets used (eqlib `PlayerClient.h`, disasm-confirmed)
`Type@0x125` (SPAWN_PLAYER=0, NPC=1, CORPSE=2 — RoF2 has one corpse type, unlike TAKP's
NPCCorpse/PlayerCorpse split), `Level@0x250` (u8), `Anon@0x2b8` (int; 1=anon, 2=roleplay,
tested as `>>1 & 1`), `PvPFlag@0x349` (u8), `GuildID@0x34c` (i32, -1 = unguilded),
`MasterID@0x38c` (pet owner spawn id, N2), `AFK@0x3c0` (int), `Linkdead@0x3d0` (u8),
`LFG@0x440` (u8), `Animation@0x10c0`. Globals: self `*(Entity**)0xDD2630`,
target `*(Entity**)0xDD2648`, controlled `0xDD2644`, CDisplay `0xDD2660`, CRaid `0xDD2690`.
**The vendored `game_functions`/`game_addresses` globals stay TAKP and are NOT used here** —
the module reads raw RoF2 offsets like chase_cam/mouse_mods.

### Status — N1 (tint) CONFIRMED WORKING; N2 (text) IMPLEMENTED, awaiting in-game test
Both detours off by default; each bails while its options are off (zero behavior change).
Our detour bodies run inside `rcp_guard::run(...)` (see crash-handler notes) so a stale
entity/actor degrades to a swallowed frame instead of crashing the client.

**N1 tint** — detour `0x58BF00`; when a coloring option is on, compute an `0xRRGGBB` and apply
via the actor vtable, return 1; else fall through to the original.
- **`concolors`** — con-level tint for players/NPCs. Level-band table reproduced locally from
  `Rcp::Game::GetLevelCon` (that fn routes through `get_user_color`/the unconstructed options
  UI, so it's unsafe to call here) → hardcoded con RGBs.
- **`colors`** — player state: PVP/AFK/LD/LFG/roleplay/guild(my/other/adventurer) + corpse.
  **Group & raid highlighting deferred to N1b** (needs the RoF2 group member list + `CRaid`);
  those players currently fall through to guild coloring.
- **`targetcolor`** / **`targetblink`** — highlight the target (`0xDD2648`), optional self-
  contained fade (own clock, GetTickCount).
- Colors are hardcoded defaults; wired to the options window in **N6**.

**N2 text** — detour the **actor's `vtable[+0x18c]`** set-string method (`__thiscall(actor, int
flag, char* text, char* scratch)`), whose address is read from a live actor
(`(*(void***)(*(void**)(entity+0x101c)))[0x18c/4]`) and hooked lazily in-game. **This is the
universal seam:** `SetNameSpriteState` calls it inline in the *normal* path (`0x58efe7`, args
`(0, &name, &scratch)`), and the `SetNameSpriteString 0x58BE90` wrapper calls it for the
*extended-nameplate* path. **Dead end that cost two rounds:** hooking `0x58BE90` catches only
extended mode — gated at `0x58E502` on `[0xD1FC34]!=0 && [*0xE67CCC+0x5c8]==1` — and stock RoF2
runs the normal path, so that hook never fired (log showed forced rebuilds but zero string
calls). Detouring the vtable function catches both modes. We map the `this` actor back to
self/target by comparing `*(void**)(entity+0x101c)`, and transform the text; other actors pass
through untouched, so the client's own name/title/guild formatting is preserved — no need to
reproduce Zeal's `generate_nameplate_text`.
**The two string params are not redundant:** the clear path (`0x58e319`) passes `(0, NULL, "")`
and the set path passes `(0, name, copyOfName)`. param2 = presence/identity (empty → sprite
removed — why hideself worked via param2 alone); **param3 = the text the renderer draws**, so
the transform must be written to BOTH params (markers were invisible while only param2 was
modified). Also: `SetNameSpriteState` early-outs at `0x58e2dd` unless `bDisplayNameSprite
(entity+0x1228)` is set — 0 for NPCs after spawn — so a forced rebuild must set it to 1 first
(self keeps 1, which made hideself work while target markers didn't).
- **`targetmarker`** — `>>Name<<` on the current target.
- **`targethealth`** — append ` NN%` on the target (`(HPCurrent@0x2e4 * 100)/HPMax@0x2dc`, or
  raw `HPCurrent` when max is 0, as for percent-only NPCs).
- **`hideself`** — blank your own nameplate (empty string) unless it's your target.
- **Deferred**: hide-raid-pets (needs raid list), show-pet-owner-name (needs a RoF2 spawn-id→
  entity lookup for the owner's name — vendored `EntityIdArray 0x78c47c` is TAKP), inline guild.

**Forced text rebuild (critical).** Unlike the tint (called every frame), the client only
rebuilds nameplate *text* on its own slow timer — NOT on targeting or HP change — so toggling a
text option or changing target had no visible effect. Fix: the tint hook (`0x58BF00`, per-frame
per-nameplate) also polls, and when our target/self/target-HP changes it **re-drives
`SetNameSpriteState 0x58E2D0`** (`__thiscall(this, bool show)`, `ret 0x4`) itself, which re-runs
our string+tint transforms immediately. `show` = the actor's own is-name-shown query
(`actor->vtable[+0x1a4](0)`) so we never conjure a nameplate the user turned off. A `g_refreshing`
guard stops the re-entrant tint call from recursing; the prior target gets one cleanup rebuild to
strip its marker on target change/deselect. (This is the RoF2 analog of Zeal's
`SetNameSpriteTint_UpdateState` RealRender promotion, done from the tint hook instead of by
patching the render call sites.)

### Nameplate roadmap (remaining)
- **N3 — extended shownames + AA titles + cache hygiene** (find RoF2 equivalents of Zeal's
  TAKP byte patches `0x4B0B3D` / `0x4FF8FF`; entity-destructor cache flush; target fast-update).
- **N4 — advanced custom fonts** (health/mana/stamina bars, drop shadows) — needs a render/
  present callback (the vendored `CallbackManager` isn't constructed and is TAKP) + a D3D**9**
  SpriteFont + world-to-screen (Zeal's `bitmap_font.*`/`default_spritefont.*` are D3D8).
- **N5 — `/tag` system** (tag arrows D3D8→D3D9 + gsay/rsay/chat-channel broadcast hooks).
- **N6 — options-window integration** (`EQUI_Tab_Nameplate.xml`, wire the color pickers).

---

## Crash + windowed-start diagnostics (`src/crash_handler.*`, `src/window_watch.*`) — IMPLEMENTED, awaiting in-game test
Added to chase the report: *"after the client crashes, a WindowedMode=TRUE relaunch flashes
the window open then hides it (not on the taskbar), the process lingers, and it self-heals
after a while; fullscreen always works."* Environment: **wine-staging 11.12 + DXVK (Vulkan)
d3d9 on NVIDIA, presented through XWayland under KWin**, no Wine virtual desktop.

### FINAL diagnosis (verified live, fix shipped) — the window gets UNMAPPED; the game is fine
Two earlier theories died on the evidence ("leftover eqgame holds GPU/wineserver" — disproven by a
clean-state repro; "windowed-init livelock" — disproven by `Logs/dbg.txt`). The truth:

- **In windowed mode the game runs perfectly** — `Logs/dbg.txt` (the client's own init journal,
  ALWAYS check this first) shows device init, fonts, strings, UI, all the way to the **login
  screen** (`eqlsUIConfig.ini loaded`), rendering ~200 fps. What I'd read as a "livelock" in the
  `+x11drv` trace was simply the login screen presenting frames.
- **The X11 window gets UNMAPPED shortly after creation** (and was parked at +946+1473, outside
  the visible area of the 2560×1440 monitor; X span is 6066×2880 multi-monitor). KWin drops
  unmapped windows from management → no taskbar entry, no alt-tab → "flashes open then hides,
  process remains". Verified with a raw-X probe: `state=UNMAPPED override_redirect=0`.
- **One `XMapRaised` fixes it**: KWin re-adopts the window (appears in `workspace.windowList()`,
  activates, behaves like any native window). Verified end-to-end through `launch-eq.sh`.
- Environment-independent within this stack: repro'd under wine-staging 11.12 **and** GE-Proton11,
  DXVK **and** WineD3D, any window size — the unmap is a Wine↔KWin/XWayland launch race, not a
  runtime version issue. gamescope also works (game window lives in gamescope's own nested
  Xwayland, no KWin management involved) — kept as `launch-eq-gamescope.sh` plan B.

**Why every in-`.asi` diagnostic missed it:** the `.asi` loads via Miles at sound init, and a
hidden/never-focused client doesn't get that far audibly (plus the test-harness shells never
attach the `.asi` at all — even fullscreen — so **PROCESS_ATTACH is NOT a valid progress signal
from scripted shells; use `Logs/dbg.txt`**). In the user's own sessions the mod attaches ~8s in.

**The fix — `tools/eq_window_fix.c` → `eq-window-fix` (host gcc + libX11, built by `make`,
installed by `make install`):** a per-launch watcher (started by `launch-eq.sh`, 120 s) that scans
ALL top-level `eqgame` class windows (skipping Wine's 1×1 override-redirect IME helpers) and
re-maps + activates any UNMAPPED one (respecting `WM_STATE=IconicState` so user-minimized clients
stay minimized), and re-centers any window parked fully outside the X span. Idempotent; multiple
concurrent instances are harmless. Log: `eq-window-fix.log` (appended) in the game dir.

**MULTI-BOX CONSTRAINT (user requirement):** multiple simultaneous eqgame.exe clients are a
supported workflow. **Never blanket-`pkill eqgame.exe` and never run `wineserver -k` in any
automation** — both kill every boxed character. The launchers only do that under an explicit
`--clean` flag; the watcher fixes every client's window and touches nothing else. Recovery for a
single wedged client: `pgrep -af eqgame.exe` → `kill -9 <that pid>`.

### `crash_handler.*` — post-mortem + a MinGW-safe detour guard
- **`crash_handler::install()`** (called first in `dllmain on_attach`): `SetUnhandledExceptionFilter`
  (full dump: exception code, faulting **module+offset** — flags our own `.asi` — access r/w+addr,
  registers, 16 bytes @ EIP, `StackWalk64` via **dbghelp** with an EBP-chain fallback) +
  `AddVectoredExceptionHandler(1,…)` used **only** for guard recovery so the game's own SEH is
  never disturbed. Logger flushes per line, so the dump survives an immediate exit. Needs
  `-ldbghelp` (Makefile) / `dbghelp.lib;psapi.lib` (vcxproj).
- **`rcp_guard::run(name, lambda)`** — GCC/mingw does **NOT** implement MSVC `__try/__except`
  (verified: `i686-w64-mingw32-g++` rejects it), so crash containment for our detours is built
  from `setjmp` + a thread-local guard-frame stack; the vectored handler `longjmp`s back **only
  on `EXCEPTION_ACCESS_VIOLATION` while a guard is armed**. Guarded bodies must be POD-only (the
  long jump abandons the frame without running destructors). Wraps the POD bodies of the
  nameplate tint (`0x58BF00`), chase Cam6 positioner (`0x799140`, incl. the eqgraphics collision
  call), and mouse `GetDeviceState` turn. Master switch `[Window] GuardDetours` (default **on**);
  `/rcpwindow guard on|off`. Caveat: the VEH precedes frame-based SEH, so guarding a body that
  calls into game code that *intentionally* AV-and-handles could steal it — those features are
  opt-in, so containment > that small risk; disable per-feature via the toggle if needed.

### `window_watch.*` — windowed-start diagnostics + opt-in self-heal (`/rcpwindow`, `[Window]`)
Driven from `ProcessGameEvents_hk` (the EndScene hook in `directx.cpp` is **dormant** — never
installed in the foundation subset — so health logging runs on the main loop instead).
- **`install_early()`** (on_attach): logs `DISPLAY`, the `eqclient.ini` window keys
  (`WindowedMode`/`Maximized`/`WindowedWidth×Height`/`WindowedMode{X,Y}Offset`), and a **Toolhelp
  scan for leftover sibling `eqgame.exe`** — the prime suspect, logged loudly at every startup.
- **`on_frame()`**: finds the main window via **`EnumWindows`-by-PID** (this build ships **no
  `eqw.dll`**, so `get_game_window()` is null), subclasses its WndProc to log
  activate/show/size/focus/destroy, and emits a per-frame health line
  (`vis/icon/fg/loop=<mainloop count>/rect vs monitor`) — dense for ~20 s then on state-change.
  `loop` climbing while `vis=0` ⇒ "alive but hidden"; `fg=0` persisting ⇒ never got foreground.
- **Opt-in self-heal** (`[Window] SelfHeal`, default **off**; `/rcpwindow on`): for the window's
  first ~8 s, if hidden/iconic/unfocused, `clamp on-screen + SW_RESTORE/SW_SHOW +
  BringWindowToTop + SetForegroundWindow` (≤4×/s). **Only helps if the client reached its main
  loop** — the pre-`.asi` hang above is out of reach.
- Commands: `/rcpwindow [on|off]` (self-heal), `guard on|off`, `verbose on|off`, `heal`, `info`.

**Status:** windowed launch FIXED via `eq-window-fix` (see FINAL diagnosis above) — verified
end-to-end through `launch-eq.sh` (window re-mapped at t+9s, KWin-managed, login screen live).
The `window_watch` module's self-heal can't address the launch-time unmap (the `.asi` loads too
late) — it stays as an in-game diagnostic; the `crash_handler` half remains the tool for the
other original complaint, genuine *in-game* crashes: the next real crash writes a module+offset
post-mortem to `rof2ClientPlus.log`. If the window ever lands awkwardly on the multi-monitor
span (dead zones between mixed-size monitors), extend `eq_window_fix.c` with RandR per-monitor
placement.
