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
- The Makefile now uses `-MMD -MP` + `-include *.d` header-dependency tracking. Before that it did
  NOT track headers, so editing a shared header (e.g. bumping a constant like `kRoleCount` in
  `rcp_options_ui.h`) without also touching its `.cpp` left a STALE `.o` — which shipped a mismatched
  build (symptom: new color-role buttons unmanaged/uncontained). If ever in doubt, `make clean`.
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

- `get_region_from_pos` (water/region tint for the camera).
- Independent-yaw mouse input in chase mode (the smoothed delta — reuse Phase 1's
**Status — WORKING (user-confirmed); confirmed addresses above retained as reference:**
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
toggle-cam binds; wire the options window to the camera settings.

---

## Nameplate port (Zeal `nameplate.cpp` → RoF2) — ALL DONE (N1 tint, N2 text, N4 font/bars, N6 options)

### N4 FINAL render seam (confirmed working in-game, 2026-07-08)
The custom nameplates draw **inside the scene, pre-UI**, giving pixel-perfect UI occlusion + world
depth occlusion (same behavior as native name-sprites):
- `CRenderInterface::SetRenderCallback` (pRender=`*(void**)0x15D46A4`, vtable **index 50**/+0xC8) is
  registered as a **frame marker only** — disasm proved it fires at `0x10097733` inside
  `CRender::RenderScene` (0x10097420, vtable +0xa8) **BEFORE the world raster** (world/actor draws +
- The actual draw is a **detour on `C2DPrimitiveManager::Render`** (EQGraphicsDX9.dll RVA 0xAE370,
  `__thiscall`, 2 dword args, ret 8; instance global 0x101B88B8; called twice per scene @0x10097817
  and @0x100978bb). On the FIRST call of a marked frame (flag from the render callback) the plates
  render: after all 3D (depth buffer = full world → wall occlusion), before the UI rasterizes
  (windows paint over plates). Install lazily from the EndScene callback once pRender exists; address
  computed as `GetModuleHandleA("EQGraphicsDX9.dll") + 0xAE370 - 0x10000000`.
- Device = `*(IDirect3DDevice9**)(pRender+0xF08)`. `bitmap_font.cpp` also unbinds vertex/pixel
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

### Nameplate roadmap (remaining) — N4 custom-font / bars overhaul (ONLY remaining nameplate work)
Draw our OWN nameplate text with a custom font and add **health / mana / stamina bars**
(+ optional drop shadows). Big infra lift, but the research below de-risks it substantially.

Already DONE (not remaining): group/raid coloring (N1b), name generation + extended shownames
(N3), options-window integration + color pickers (N6) — see the memory notes for their RE.
DROPPED from scope (per user): the `/tag` system (old N5) — no tag arrows.

#### RESOLVED: RoF2 is a **D3D9** client (settles the whole approach)
- Game dir ships BOTH `EQGfx_Dx8.dll` (imports `d3d8.dll`, LEGACY/dead) and
  **`EQGraphicsDX9.dll` (imports `d3d9.dll` + `d3dx9_30.dll`)** — the DX9 one is the live
  renderer. **`eqgame.exe` itself imports `d3dx9_30.dll`** (winedump-confirmed). The
  "eqgfx_dx8.dll" name in the vendored `callbacks.cpp` is TAKP-era and misleading — ignore it.
- Device is **`IDirect3DDevice9`** (eqlib `CRender`: `IDirect3D9* @0x0f04`, `IDirect3DDevice9*
  pD3DDevice @0x0f08`, err string "Direct3DCreate9 failed").
- **Why this is the easy path:** Zeal's font engine calls `D3DXCreateTexture`,
  `D3DXVec3Project`, `D3DXMatrix*` — all present in `d3dx9_30` with IDENTICAL signatures. The
  port is the mechanical D3D8→D3D9 delta below, NOT a rewrite of the font logic.

#### Zeal font architecture (source: `bitmap_font.*`, `nameplate.cpp`, `floating_damage.cpp`)
- Custom bitmap-atlas engine (`BitmapFontBase` + `BitmapFont` 2D / `SpriteFont` 3D), NOT
  `ID3DXFont`. Atlas = a MakeSpriteFont `.spritefont` binary (magic `"DXTKfont"`), DXT2
  texture via `D3DXCreateTexture`; embedded fallback `default_spritefont[]` (arial_8).
  Glyphs `glyph_table[128]`; own dynamic VB (FVF) + static IB, batched `DrawIndexedPrimitive`.
- **Nameplates = 3D billboard** (`SpriteFont`): hand the entity HEAD world-pos to the GPU;
  WORLD = scale(0.025) · transpose(view 3x3) · translate(headPos); z-buffer occludes it. **No
  explicit world-to-screen.** Head pos in TAKP = `entity->ActorInfo->DagHeadPoint->Position`
  (RoF2 equiv TBD — see AvatarHeight@0x138 / actor bounding-radius vtable in Phase-2 notes).
- **Floating damage = 2D** (`BitmapFont`): uses `DirectX::WorldToScreen` (`D3DXVec3Project` on
  the device's GetTransform WORLD/VIEW/PROJECTION + GetViewport), note EQ axis-swap x<->y.
- **Bars = special glyphs** `\x01`(bg) `\x02`(hp) `\x03`(mana) `\x04`(stam): appended to the
  string as control chars sampling an injected solid-white 4x4 texel block; fill width =
  pct·barWidth(120), height 6; HP color banded green/yellow/orange/red, mana blue, stam gold.
  Percents pushed via `set_hp/mana/stamina_percent` before `queue_string`.
- **Drop shadow / outline = re-queue** the same glyphs in near-black `XRGB(1,1,1)` at a small
  offset (outline = 2nd opposite offset); bars skip the shadow pass.

#### RoF2 render seam (source: our `directx.cpp` + eqlib graphics headers)
- **`src/directx.cpp` ALREADY has a correct D3D9 EndScene vtable hook** (throwaway-device trick:
  `Direct3DCreate9` → hidden HAL device → grab vtable **slot 42 = EndScene** → `swap_vtable_entry`
  in `src/hooks.cpp`). It's DORMANT only because **`directx::install()` is never called** —
  under DXVK all devices from the one `d3d9.dll` share a vtable, so this fires for the real device
  too. Activation = one guarded call from the `ProcessGameEvents_hk` magic-static (`dllmain.cpp`).
  This seam is **offset-independent** (doesn't need the uncertain game structs) → use it first.
- **`CallbackManager` (`src/callbacks.cpp`) is dead TAKP code** — unconstructed, all-TAKP
  addresses, its RenderUI hook even resolves the legacy `eqgfx_dx8.dll`. Do NOT wire it up.
- Cleaner device handle (optimization, later): `CRender* r = *(CRender**)0x15D46A4;` then
  `dev = *(IDirect3DDevice9**)((char*)r + 0xF08)`. **CAVEAT: eqlib tracks a NEWER build than our
  2013 exe** — the collision code already found disasm beats eqlib for `0x15D46A8/0x15D46B0`, so
  disasm-verify this pointer + `+0xF08` before trusting it. The EndScene trick avoids the risk.
- World-to-screen options: `CCameraInterface::ProjectWorldCoordinatesToScreen` (camera vtable
  ~slot 29 / +0x74, VERIFY slot on 2013 binary) OR manual `matrixViewProj` from `CRender+0x16c0`.
  Also `CDisplay::WriteTextHD2(text,x,y,color)` exists as a native screen-space writer.

#### D3D8→D3D9 code delta to apply when porting `bitmap_font.*` (from the Zeal audit)
- `SetVertexShader(FVF)` → **`SetFVF(FVF)`**; `SetStreamSource(0,vb,stride)` → add offset arg
  **`(0,vb,0,stride)`**; `SetIndices(ib,base)` → **`SetIndices(ib)`** (base dropped);
  `DrawIndexedPrimitive(...)` → **insert `BaseVertexIndex` as 2nd arg** (trickiest — the batch's
  start-vertex becomes BaseVertexIndex, MinIndex=0); `CreateVertexBuffer/IndexBuffer` → add
  trailing **`NULL`** (pSharedHandle); `Lock` ppbData `BYTE**`→**`void**`**; texture-stage
  **`D3DTSS_MINFILTER` → `SetSamplerState(0, D3DSAMP_MINFILTER, …)`** (COLOROP/ALPHAOP stay).
  Swap the bundled `d3dx8/` SDK for **d3dx9** headers/lib (`d3dx9_30`). Everything else (FVF
  flags, DXT2/INDEX16 formats, pools, lock flags, the −0.5 half-texel offset, billboard math,
  `D3DXVec3Project`) is unchanged D3D8↔D3D9.

#### N4 implementation plan (testable opt-in increments, off by default)
- **N4a — render seam: IMPLEMENTED + DEPLOYED, awaiting in-game log check.** `directx::install()`
  is now called once from the `ProcessGameEvents_hk` magic-static (`dllmain.cpp`). VERIFY in-game:
  `grep -E "directx::install|EndScene hook fired" /home/joshua/Games/RoF2/rof2ClientPlus.log`
  should show `install() -> OK` then periodic `EndScene hook fired, frame N`. No visible change.
  TODO after confirm: wrap `hkEndScene` body in `rcp_guard`; add `Reset`/device-lost handling.
  FOUNDATION — de-risks everything downstream, no game-offset dependency.
- **N4b — font engine: IMPLEMENTED + DEPLOYED, awaiting in-game test.** Ported `bitmap_font.{h,cpp}`
  + `default_spritefont.{cpp,h}` to D3D9 (self-contained; Zeal deps replaced with logger / a local
  split_text / a module-relative fonts dir / the RoF2 screen-res globals `0x00798564/8`). D3DX comes
  from MinGW's own `libd3dx9_30.a` + `<d3dx9.h>` (no vendoring needed; `-ld3dx9_30` in the Makefile) —
  matches the game's `d3dx9_30.dll`. Applied the full D3D8->D3D9 delta (SetFVF, +offset on
  SetStreamSource, SetIndices base dropped, `DrawIndexedPrimitive` BaseVertexIndex=0 inserted [our IB
  indices are ABSOLUTE so base stays 0], `Create*Buffer` +NULL, `Lock` void**, new `D3DSamplerStateStash`
  for `D3DSAMP_MINFILTER`). New `directx.cpp` render-callback list (invoked each EndScene, rcp_guard-
  wrapped) + `get_device()`. New `font_overlay.{h,cpp}` module (constructed in `rcp.cpp`) draws a 2D
  test string. VERIFY in-game: `/rcpfont test on` shows a yellow "rof2ClientPlus font test ..." string
  top-left; `off` removes it. (Embedded font is arial_8 — small; a bigger `.spritefont` in
  `<game>/uifiles/rcp/fonts/` can be loaded by name later.) TODO: device-lost `Reset` handling (VB/IB
  are D3DPOOL_DEFAULT); currently no `IDirect3DDevice9::Reset` hook.
- **N4c — nameplate integration: IN PROGRESS (3D billboards).** De-risk probe `/rcpfont test3d`
  CONFIRMED in-game. **RESOLVED:** (1) render-space head position = the entity's three world floats
  in MEMORY ORDER `(0x64,0x68,0x6c)` passed straight through, 0x6c vertical (+ `AvatarHeight@0x138`) —
  no axis swap; (2) world VIEW/PROJECTION are STILL LIVE at EndScene (2D UI uses XYZRHW, doesn't
  disturb them) so NO mid-frame matrix capture needed. Ship Zeal's `.spritefont` atlases
  (`uifiles/rcp/fonts/`, installed by the Makefile); load **`arial_bold_24` @ scale 0.025** (crisp;
  the embedded arial_8 upscaled looked blocky). **Placement:** anchor at head-top
  (`feet + AvatarHeight@0x138`) + a **screen-up (billboard-local) lift** `set_screen_offset` — NOT a
  world-Z offset (that collapses when viewed top-down). User-tuned default **1.8 line-heights**
  (`/rcpfont offset <lines>` live-tunes). A pixel-perfect match to the native plate isn't possible
  (native is 2D screen-projected) but it's moot once native is suppressed.
- **N4c v1 (tint-hook enumeration) — SUPERSEDED.** First cut reused the `SetNameSpriteTint 0x58BF00`
  hook to enumerate, but that hook fires per-frame only for **self + target** (they get continuous
  re-tint/highlight); other PCs/NPCs are tinted only occasionally, so only self+target drew. Also
  showed only `DisplayedName` (no title/last/guild), white (no con color), and empty self bars (spawn
  HP/mana/end fields are the *target/NPC* percent path — 0 for the local player).
- **N4c v2 DONE + DEPLOYED (spawn-list enumeration), awaiting in-game test.** `/rcpfont on|off`.
  Enumeration now WALKS THE SPAWN LIST directly each EndScene (research Q1, triple-confirmed):
  `mgr=*(void**)0xE641D0`, `first=*(void**)(mgr+0x08)`, `next=*(void**)(entity+0x08)`, NULL-term.
  Per entity: skip `Type@0x125==2` (corpse), require `actor@0x101c != 0`, distance-cull vs self
  (`g_np_max_dist`=300). Text + color REUSE nameplate.cpp via new `nameplate::billboard_text()`
  (generate_player_name for PCs → title/first/last/guild/AFK per /shownames; DisplayedName for NPCs)
  and `nameplate::billboard_color()` (state_color + con for NPCs + client-like default). HP bar: self
  from the **character profile** (Q5 — `pLocalPC=*(void**)0xDD261C`, czc=pc+0x2DC8; `Cur_HP 0x449E00`,
  `Max_HP 0x443FA0`, `Cur_Mana 0x4442E0`, `Max_Mana 0x581E60`, `Max_Endurance 0x582020`, cur-endurance
  = `*(int*)(profile+0x3390)`, profile via `GetCurrentProfile 0x7DB210(czc+0x0C)`); others from spawn
  `HPCurrent@0x2e4`/`HPMax@0x2dc` percent (drawn only when >0). Mana+stam bars self-only.
  **Refinements (all awaiting in-game test):**
  - **Anchor = model HEAD_NAME point (research-confirmed).** The native name sprite is attached (in
    EQGraphicsDX9.dll) to the model's `"HEAD_NAME"` skeletal point, which SCALES PER MODEL — so
    feet+`AvatarHeight@0x138` was "way off" for NPCs. The live world anchor is readable at
    `ss=*(actor+0x204)` (CStringSprite), pos `ss+0x6c/0x70/0x74` (0x74 vertical). font_overlay now
    reads that (fallback feet+AvatarHeight if `ss` is null pre-name). Native's own extra lift is
    `ss+0x60` (0 for HEAD_NAME, 1.55 else) added along camera-up — our `set_screen_offset` is the
    analog (default lowered to 0.5 lines now the anchor is the head, not the feet).
  - **Sizing = fixed world scale (shrinks with distance, like native).** Native is a WORLD billboard
    with a distance-compensated + CLAMPED scale (base 0.325 gfx units), NOT 2D screen text. A
    distance-compensation attempt (scale ∝ dist-to-camera for constant screen size) made far plates
    GIANT (bad camera read / euclidean-vs-depth), so REVERTED to a plain fixed world scale
    `g_np_scale`(0.04, `/rcpfont scale <n>`) that shrinks naturally with distance. SpriteFont has
    per-string scale support (`GlyphString.scale`) retained for later. TODO if wanted: native-style
    min/max screen-size clamp. `set_align_bottom(true)` so the plate grows UPWARD from the head anchor
    (adding HP/mana/stam bars no longer pushes the name down onto the head).
  - **Group coloring fix:** `is_group_member` now requires >=1 OTHER pc (the client keeps a stale solo
    group struct, which had colored your own plate as grouped).
  - **Native suppression DONE.** `nameplate::set_suppress_native(bool)` — when billboard mode is on,
    `transform_entity` returns "" for EVERY entity (blanks the native name via the set-string
    vtable+0x18c hook, which per research KEEPS the con-tint firing). font_overlay drives it on
    enable/disable. Native disappears within the client's own rebuild cycle (no forced rebuild yet).
  - **Persistence DONE.** `[Font]` ini section: `Billboard` (on/off), `Offset`, `Scale`. Loaded in
    the FontOverlay ctor (applies suppression to match), saved on every `/rcpfont` change and on the
    options-window toggle. Default scale is **0.032** (was 0.04; user asked for ~20% smaller).
  - **Options-window toggle DONE.** Standalone "Custom nameplates (3D font + bars)" checkbox on the
    Nameplate tab (`Rcp_NpBillboard`, generated by `tools/gen_rcp_options_ui.py`, wired in
    `rcp_options_ui.cpp` → `font_overlay::get_enabled`/`set_enabled`).
  **Still to do (N4d polish):** device-lost `Reset` handling (VB/IB are D3DPOOL_DEFAULT); optionally
  a native-style min/max size clamp, and persisting these to the options UI as sliders. Full
  name-sprite RE (CActor vtable 0x10137074, HEAD_NAME literal 0x10137c94, CStringSprite layout) is in
  the memory notes.
- **N4d — options + polish:** font pick, bar/shadow toggles on the Nameplate tab; ini persistence.

---

## Keybinds port (Zeal `binds.cpp` → RoF2) — DONE / WORKING
Lives in `src/keybinds.cpp` (the `/rcpbinds` feature), self-contained raw-RoF2 module. The
vendored `src/binds.cpp` is a TAKP reference only (its patch sites `0x52507A` etc. are TAKP)
and stays unconstructed.

### Key architecture finding — RoF2 ≠ TAKP bind-table model
- TAKP had a 128-name table with free index gaps and per-site hardcoded array refs that Zeal
  repointed to a 256-entry copy. **RoF2's `BindList` (`char*[479]` @ `0xACBEE8`, .data) is
  DENSE (0..478 all named)** and every key array is a fixed 500 slots — there is no room to
  append, and indices 479–499 are nameless internal dispatch ids (INSERT/CAMP/charselect…).
- **So we HIJACK dead commands instead**: the `CMD_REAL_ESTATE_*` block (429–446, the housing
  UI that doesn't exist on emu) → our binds get native ini persistence, native key capture,
  and native Options→Keys rows for free. We use **429–443**; 444–446 spare.
- The options Keys page only allows assignment for command ids **≤463** (`cmp eax,0x1CF` @
  `0x70ACCB`, the ONLY such check) — 429–443 is safely inside; nothing needed patching.

### Confirmed stock-RoF2 addresses (eqlib + disasm-verified)
- **`BindList` `0xACBEE8`** (`char*[479]`); count 479 (`0x1DF` bound at `0x4D71EA`).
  `GetMappableCommandName 0x4D7190`, `FindMappableCommand 0x4D71A0`.
- **`ExecuteCmd` `0x4D7230`** — `__cdecl(cmd, keydown, data, combo)`; keyboard sites push 3
  args, some UI sites 4 (forward all 4 in the detour). Gated on `g_eqCommandStates[500]` @
  `0xDCEF08` (0 = suppressed) — our detour mirrors that gate.
- **`KeypressHandler*` @ `0xE639B0`** (created by `0x55B2E0`, `new(0x1194)`): layout
  `KeyCombo NormalKey[500]` @+0, `AltKey[500]` @+0x7D0, `char CommandState[500]` @+0xFA0.
  `KeyCombo = {u8 Alt, u8 Ctrl, u8 Shift, u8 DIK}`; persisted int = `dik | alt<<28 |
  ctrl<<29 | shift<<30`. Methods: `HandleKeyDown 0x5594E0` (routes to CXWndManager first —
  typing protection comes free), `HandleKeyUp 0x559800`, `LoadAndSetKeymappings 0x55B100`,
  `ResetKeysToEqDefaults 0x5598D0` (reads `[Defaults] UseWASDDefault`), `SaveKeymapping
  0x55AC50` (ALL bind-change persistence funnels here — its only callers are the two Attach
  fns `0x55AFA0`/`0x55AFE0` + `MapKeyToEqCommand 0x55AD50`), `DeleteAllKeymappings 0x55B020`,
  `GetEqCommandSaveName 0x5596F0`. NOTE eqlib's two `Attach*` defines are swapped for this
  build (0x55AFA0 = primary/`_1`, 0x55AFE0 = alternate/`_2`).
- **Persistence**: `eqclient.ini` `[KeyMaps]`, key `KEYMAPPING_<BINDLISTNAME>_<1|2>`, decimal
  combo int; global per install; written only on change.
- **Options window**: `COptionsWnd*` @ `0xD1FC6C`, object size 0x1378, ctor `0x70AFF0`.
  `InitKeyboardAssignments 0x7046C0` (giant, ~421 inlined adds: `CStringTable::GetString
  0x7D0660` on instance `[0xDD25C4]` → `CXStr::operator=(char*) 0x805DE0` → category int)
  fills the `{CXStr desc, int category}` stride-8 array @ **this+0x40C** (capacity exactly
  479; eqlib's `Binds[0xA1]`@0x228 is stale). Real-estate ids 429–432 natively get category
  0x40/UI labels, 434–445 get 0x40000/Item Placement, 433+446 get none — all overwritten by
  our detour. `RefreshCurrentKeyboardAssignmentList 0x70A560` (`__thiscall`, no args)
  repaints the list; the client itself calls it via `[0xD1FC6C]` (e.g. at `0x5FD10F`).
  **Categories are BITMASKS** (filter choice i = bit i): Movement 0x1, Commands 0x2, Spell
  0x4, Target 0x8, Camera 0x10, Chat 0x20, UI 0x40, Hotbar1..10 0x80..0x10000, VoiceChat
  0x20000, ItemPlacement 0x40000, "All" = 0x7FFFF.
- **Merchant stack buy/sell**: `CMerchantWnd*` @ `0xD1FCA4` (size 0x2B0); selected slot
  `ItemGlobalIndex.Location` @+0x23C (**9 = merchant side → buy, 0 = possessions → sell**),
  `pSelectedItem` @+0x248; active merchant entity @ `0xDD264C`. `HandleBuy 0x6F2620` /
  `HandleSell 0x6F29C0` (`__thiscall(int qty)`, −1 = quantity flow) → `CQuantityWnd::Open
  0x725B00` reads **`CXWndManager` (`[0x15D3D00]`) shift byte @+0x9D** and with shift set
  commits the max quantity synchronously without showing the dialog → `WndNotification`
  msg 0x24 → PurchasePageHandler `RequestGetItem 0x6F5BE0` / `RequestPutItem 0x6F6650`.
  So the bind fakes shift around `HandleBuy/-Sell(−1)` — the client computes the full stack
  (min of stock & stack size / current stack count) and runs its own money/space checks.
- **Slash commands confirmed in the command table** (`__CommandList 0xACD5A8`, stride 0x1C):
  `/autoinventory` (handler `0x4FE8C0`), `/autofire` (`0x4FB9B0`), `/pet` (`0x4E2150`),
  `/loot` (`0x4E7880`), `/assist`. RoF2 `/pet` subcommand vocabulary (string table @ file
  offset 0x5f3514): attack, qattack, back off/back, follow, guard, sit, hold, ghold,
  spellhold, taunt, regroup, stop, focus, feign, leave, get lost, health/report health,
  leader/who leader.

### What was OMITTED because stock RoF2 already has it (user rule: skip native features)
Strafe (`STRAFE_LEFT/RIGHT` 6/7), tab-target cycling (`CYCLENPCTARGETS` 33 +
`TARGET_PREV/NEXT_NPC` 474/475), PC cycling (31), **corpse target/cycle (34/35)**, map
toggle (`TOGGLE_MAPWIN` 338), reply target (`RTARGET` 317), toggle-last-two-targets (343),
open/close/toggle all bags (379–381), camera selects (345–350), 10 native hotbar pages,
stop-cast (370). Zeal's map/tellwindow/slowturn/rangeattack/page-10 binds don't apply (no
such modules here / native equivalents); deferred: single-shot Range Attack, slow-turn.

### Status — DONE / WORKING; all detours act only on hijacked ids
- **14 new binds** (ids 429–443): Pet Attack/Back Off/Follow/Guard/Sit/Health/Hold (forward
  `/pet …`), Auto Fire (`/autofire`), Auto Inventory (`/autoinventory`), Buy/Sell Stack
  (merchant fake-shift), Assist (`/assist`), Toggle Nameplate Colors / Con Colors / Hide
  Self (drive `nameplate_settings`), Loot Target (`/loot`). Set them in the STOCK Options →
  Keys tab (categories Commands/Target/UI).
- Detours: `ExecuteCmd` (dispatch + swallow), `InitKeyboardAssignments` (labels+categories),
  `SaveKeymapping` + `DeleteAllKeymappings` (per-char redirection). Constructor repoints
  `BindList[429..443]`, zeroes any stale real-estate combos the boot-time load left in those
  slots, then re-drives `LoadAndSetKeymappings` so saved `KEYMAPPING_RCP_*` keys apply.
- **Per-character keybinds** (`/rcpbinds perchar on|off`, `[Binds] PerCharacter` in
  rof2ClientPlus.ini): a `[KeyMaps_<CharName>]` overlay section in eqclient.ini. On world
  entry / char switch (frame poll): ResetKeysToEqDefaults → LoadAndSetKeymappings (global)
  → overlay. Options-window key changes while ON are redirected to the char section; the
  Keys-page "Reload defaults" wipes only the char section (global stays) and re-syncs to
  global next frame. `/rcpbinds list|reload` for inspection.

---

## Crash + windowed-start diagnostics (`src/crash_handler.*`, `src/window_watch.*`) — DONE (windowed launch FIXED; crash handler armed)
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
