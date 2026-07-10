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
- `make && make install` → drops `rof2ClientPlus.asi` (+ `uifiles/rcp/` + fonts) into `/home/joshua/Games/RoF2/`.
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

## Phase 2 — third-person chase camera — DONE
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
  `Height@0x13c`, `Type@0x125` (Player=1), `Properties@0x128` (CharacterPropertyHash =
  bodytype), `Targetable@0x160` (bool, "true if mob is targetable"), `Race@0xeb4`,
  `SpawnID@0x148`, `Mount@0x154`, `WhoFollowing(autofollow)@0xe40`, viewactor
  `mActorClient@0xea4 + pActor@0x101c` (`CActorInterface*`, all-virtual; BoundingRadius
  vtable +0x60/+0x64). Struct reference: `GitHub/eqlib/include/eqlib/game/PlayerClient.h`
  matches this client field-for-field. Nameplates hide untargetable "controller"/trigger
  NPCs (EQEmu zone_controller etc.) by blanking when `Type==NPC && !Targetable` (MQ2's
  UNTARGETABLE/TRIGGER/TRAP/TIMER buckets all reduce to untargetable).
- Phase-3 refs: `SetViewActor 0x48F030` (bad-cam msg patch: `0x48F091` `74 27`→`EB 27`),
  `GetClickedActor 0x48B6B0`, exact eye-height virtual `0x0058CF00` (thiscall).

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

**2c world-collision pull-in (DONE, in `chase_cam.cpp`)** —
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

---

## Nameplate port (Zeal `nameplate.cpp` → RoF2) — ALL DONE (N1 tint, N2 text, N4 font/bars, N6 options)

### N4 FINAL render seam (confirmed working in-game, 2026-07-08)
The custom nameplates draw **inside the scene, pre-UI**, giving pixel-perfect UI occlusion + world
depth occlusion (same behavior as native name-sprites):
- `CRenderInterface::SetRenderCallback` (pRender=`*(void**)0x15D46A4`, vtable **index 50**/+0xC8) is
  registered as a **frame marker only** — disasm proved it fires at `0x10097733` inside
  `CRender::RenderScene` (0x10097420, vtable +0xa8) **BEFORE the world raster** (world/actor draws +
  particles follow it). Drawing there = world overdraws the text (invisible with z-write off) or
  z-fails behind the billboards' depth (fog-colored glyph blobs with z-write on).
- The actual draw is a **detour on `C2DPrimitiveManager::Render`** (EQGraphicsDX9.dll RVA 0xAE370,
  `__thiscall`, 2 dword args, ret 8; instance global 0x101B88B8; called twice per scene @0x10097817
  and @0x100978bb). On the FIRST call of a marked frame (flag from the render callback) the plates
  render: after all 3D (depth buffer = full world → wall occlusion), before the UI rasterizes
  (windows paint over plates). Install lazily from the EndScene callback once pRender exists; address
  computed as `GetModuleHandleA("EQGraphicsDX9.dll") + 0xAE370 - 0x10000000`.
- Device = `*(IDirect3DDevice9**)(pRender+0xF08)`. `bitmap_font.cpp` also unbinds vertex/pixel
  shaders + disables fog/alpha-test + fully specifies sampler state (mid-scene device state is dirty,
  unlike EndScene). DEAD ENDS (do not retry): EndScene draw (over UI), depth-mask of CXWndManager
  window rects (top-level list is ~777 flat entries incl. children — can't represent opaque UI),
  drawing at the SetRenderCallback itself (pre-world), DrawWindows-entry draws (2D path, discarded).
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

### Status — N1 (tint), N2 (text), N6 (options window) all DONE/WORKING
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

## Distance-fog removal (`src/no_fog.cpp`, `/rcpfog`) — DONE (awaiting in-game confirm)

Goal: zero distance fog in every zone, day and night.

### Key finding — the TAKP fog interface does NOT port
The Zeal/TAKP `CDisplay::SetFog` (0x004add26), `SetYon` (0x004aca7f), `SetDayPeriod`
(0x004b177f) and `GAMEZONEINFO @ 0x00798784` in `game_structures.h` / `game_addresses.h` are
**stale TAKP addresses** — all decode to garbage in the RoF2 `eqgame.exe` and are referenced by
nothing. In RoF2 the zone header is a **heap object** (`new(0x23c)`, e.g. the allocs at 0x497fdc /
0x49ac16 / 0x645c59 → 0x8dbb3b), not a fixed global, so the Zeal char-select trick
(`ZoneInfo->0x184/0x194 = big; SetYon(big)`) has no fixed address to poke. The real fog-colour
setup lives around 0x4ad960–0x4adea0 (writes fog RGB to `ds:0xdcecd4/d8/dc`, init-flag `0xdcece0`),
but hooking it was unnecessary.

### Mechanism — device-level D3DRS_FOGENABLE filter (zone-layout independent)
RoF2 distance fog is **fixed-function D3D fog** gated by `D3DRS_FOGENABLE` — proven inside this mod:
`bitmap_font.cpp:816` disables that exact state so world-pass glyphs aren't "greyed out by depth".
So `no_fog.cpp` hooks the shared DXVK `IDirect3DDevice9::SetRenderState` (vtable **slot 57**; EndScene
at 42 in `directx.cpp` fixes the standard layout) and forces `D3DRS_FOGENABLE → FALSE`. The client
re-issues fog-enable every frame, so a persistent filter covers all zones + day/night with no
re-hooking; the `/rcpfog` toggle just flips a bool. Install mirrors `mouse_mods` (capture original
from the slot BEFORE `hooks->Add(..., hook_type_vtable)`, call it directly on the hot path — no
hook-map lookup). Installed lazily on the first `directx::add_render_callback` device (the shared
vtable is the same one `directx.cpp` already patched for EndScene).

`/rcpfog on` removes fog (default ON), `off` restores client default, bare toggles; persisted to
`[Fog] RemoveDistanceFog` in `rof2ClientPlus.ini`. Caveat: also removes underwater fog haze (same
FOGENABLE gate) — toggle off if that's unwanted. **Confirmed working in-game 2026-07-08.**

Also exposed in `/rcpoptions` as a **new "Display" tab** (checkbox "Remove distance fog", ScreenID
`Rcp_NoFog`): `no_fog.h` publishes a `no_fog_settings` namespace (`get_enabled`/`set_enabled`), the
UI binds/polls it like the other toggles, and the tab strip grew 4→5 (window widened 280→356 to keep
the proven 64 px tab width — see `tools/gen_rcp_options_ui.py`). Regenerate order unchanged:
`gen_option_overrides.py` then `gen_rcp_options_ui.py`.

## Target ring (`src/target_ring.cpp`, `/rcpring`) — DONE (awaiting in-game confirm)

Goal: a **solid-color "donut" ring** drawn flat on the ground under the current target, with
user-settable color (stock color picker), outer radius, inner radius (the donut hole), and opacity.
Zeal reference: `/home/joshua/workspace/GitHub/Zeal/Zeal/target_ring.cpp` (TAKP, D3D8).

### What it is (a slim subset of Zeal's target_ring)
Zeal's ring supports textures, spin, a 3-D cone/cylinder, auto-attack blink, and heading-match. The
cone and auto-attack blink are **intentionally dropped**, but the **optional graphic (texture)** plus
**spin / face-heading** were added back (see "Ring graphic" and "Spin" below). Kept: enable, RGB color
OR **con-level color** (toggle), outer radius, inner radius, opacity, hide-under-self, an optional
graphic, and spin-vs-face-heading. `/rcpring [on|off | outer N | inner N | opacity 0-1 | color RRGGBB
| con on|off | self on|off | graphic <name>|none | spin on|off]`; bare `/rcpring` toggles. Persisted
to `[TargetRing]` (`Color` stored as `RRGGBB` hex, like `[NameplateColors]`; `ConColor` the
con-vs-fixed toggle; `Graphic` the texture name or `None`; `Spin` the spin toggle). Off by default.

**Con coloring** (Zeal's `target_color` toggle): when on, the ring is colored by the target's con
level instead of the fixed color, via a new `nameplate::con_color_for(entity)` export that ALWAYS
computes the con color (independent of the nameplate con-colors setting) from the same level-band
table + user-editable con palette (Colors tab). Setting a fixed color (command `color`, or the swatch
picker) flips con off so the choice is visible.

### Vertical placement — floor Z, NOT the entity position ref
The entity `Z@0x6c` is a POSITION REFERENCE that floats above the model's feet (same quirk the N4
billboards hit: "feet+AvatarHeight was way off for NPCs"), so drawing there made the ring hover. The
native ring sits on the floor, so the vertical uses **`FloorHeight@0x28`** (eqlib `PlayerBase`; that
struct matches our build at `0x28/0x64/0x68/0x6c/0x80/0x125/0x138`) — the client's own absolute floor
Z under the entity — guarded (nonzero, and `posZ - floorZ` within `[-10, 60]`) with a fallback to
`0x6c`. Horizontal center stays `0x64/0x68`. A one-shot `[ring] Z:` log (first ~5 frames after
enable) records `pos/floor/drop/chosen` for diagnosis.

### Render seam — REUSES the nameplate billboards' pre-UI in-scene seam (not a new hook)
A ground ring wants exactly the seam the N4 billboards already solved: **after the world raster**
(so it z-tests against terrain/walls and is occluded by geometry in front) and **before the UI
raster** (so windows paint over it). That seam is the `C2DPrimitiveManager::Render` detour in
`font_overlay.cpp`. Rather than add a second detour on the same address (conflict) or draw at
EndScene (post-UI → draws over windows), `font_overlay` now exposes
**`font_overlay::add_scene_draw(cb)`**: extra world-space overlays are invoked right after
`on_render_nameplates`, inside the same marked-frame block, each `rcp_guard`-wrapped. It installs
whenever `FontOverlay` exists — independent of whether billboard nameplates are enabled. `TargetRing`
registers its draw there in its ctor.

### Geometry + coordinate mapping (trivial for a circle)
- Target = `*(void**)0xDD2648`; self = `0xDD2630`. Feet/ground position = entity floats
  `0x64/0x68/0x6c` in **memory order**, `0x6c` the vertical — **the same three floats the billboards
  translate `D3DTS_WORLD` by** (confirmed working), so the ring center lands exactly where the native
  plate anchors. A circle is rotationally symmetric, so the EQ X/Y ordering is irrelevant: generate
  it in the render horizontal plane (vary the first two render coords, hold `0x6c`) and it lies flat.
- Donut = one closed **triangle strip**: at each of `kNumSegments`(96)+1 angles emit an outer-rim then
  an inner-rim vertex; the strip stitches successive pairs into the band's quads. Built around the
  origin, translated to the target's feet via a hand-built `D3DMATRIX` (`_41/_42/_43`), lifted
  `+0.20` to avoid z-fighting the ground; **VIEW/PROJECTION stay live** from the world pass.

### D3D9 draw (same mid-scene hygiene as the 3-D billboards, minus the atlas)
Fixed-function `D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1` (the UVs are only consumed by the graphic
path; the solid path ignores them), drawn with **`DrawPrimitiveUP`** (no vertex-buffer lifecycle /
device-lost handling). Unbind vertex+pixel shaders (a bound PS overrides fixed-function), then via the
shared `D3DRenderStateStash`/`D3DTextureStateStash` (in `bitmap_font.h`): `CULLMODE=NONE`, alpha-blend
`SRCALPHA/INVSRCALPHA`, `ZENABLE=TRUE` (terrain occlusion), `ZWRITEENABLE=FALSE` (translucent, no
depth write), `LIGHTING/ALPHATEST/FOG/SPECULAR` off, stage 1 disabled. Solid mode: stage-0
`COLOROP/ALPHAOP = SELECTARG1(DIFFUSE)` + no texture. Restore all of it (incl. WORLD + shaders) after
the draw. `alpha = opacity*255` in the ARGB diffuse.

### Ring graphic (the one Zeal extra added back)
`/rcpring graphic <name>` (or the "Cycle graphic" button on the Ring tab) selects a **`.tga` from
`uifiles/rcp/targetrings`** (bare `/rcpring graphic` lists what's on disk; `none` clears it). Loaded
with **`D3DXCreateTextureFromFileA`** (defaults to `D3DPOOL_MANAGED`, so it survives device resets —
no reset hook needed) **deferred to the render thread**: the setter only assigns `g_graphic` + sets a
dirty flag, and `draw_ring` does the create/release the next frame against the live device (inside
`rcp_guard`). Failure logs once and falls back to the solid ring.

Mapping is **identical to Zeal** so its own ring textures render unchanged: Zeal ships 261×2048 RGBA
"strip" textures (u = inner..outer across the band width, v = around the circumference), and the donut
UVs are `u = 1` on the outer rim / `0` on the inner rim, `v = 1 - i/segments`. Stage-0 is
`MODULATE(TEXTURE, DIFFUSE)` for **both color and alpha** (like Zeal), so the ring color/con-color
tints the texture (white ⇒ the texture's native colors) and opacity scales its alpha; sampler set to
`LINEAR`, `ADDRESSU=CLAMP` (radial), `ADDRESSV=WRAP` (seamless around). When a graphic is active we
draw **only** the textured donut (no solid pass behind it — cleaner than Zeal's solid+texture stack).
A curated set of Zeal's ring `.tga`s ships in `uifiles/rcp/targetrings` (`make install` copies them);
users can drop in their own.

### Spin / face-heading (`/rcpring spin on|off`, `Rcp_RingSpin` checkbox)
The ring rotates about its vertical axis (local Z, the ring's normal) via the WORLD matrix's upper-left
2×2 — no vertex regen, and invisible on a solid ring (symmetric), so it only shows on a graphic. Two
modes on one toggle (default **on**): **spin** = a slow continuous rotation (`rot = -g_spin_angle`, one
revolution every `kSpinPeriodSec` = 8 s; the accumulator + `GetTickCount64` delta live on the render
thread, with the delta capped at 100 ms so an alt-tab/zone stall doesn't lurch it, and reset to 0
whenever spin is off so re-enabling doesn't jump); **face-heading** (spin off) =
`rot = Heading@0x80 * (PI/256) + g_face_offset_deg*(PI/180)`, i.e. the graphic is oriented to the way
the target faces. `Heading` is a `float 0..512` (eqlib PlayerClient, same struct as the pos fields;
`Heading@0x80` is the logical heading and it *does* update during movement — an earlier detour through
the render actor's `GetHeading()` proved identical and was reverted). The `+PI/256` sign was confirmed
in-game (the initial `-PI/256` was backwards). `/rcpring faceoffset <deg>` (`[TargetRing] FaceOffset`,
default 0) fine-aligns a texture's "front" — 0 works for DarkMode.

**Self-target caveat (inherent, not a bug):** when the target is *you*, turning rotates your camera
with the ring, so its rotation cancels on screen and it looks static — it's correctly pointing where
you face (always "into the screen"). NPC / other-player targets turn independently of your camera, so
the ring visibly tracks them — that's the useful case. (This chewed several debug rounds: a temporary
bright-yellow front pointer + a self-vs-NPC diagnostic log finally isolated it to a reversed sign, both
since removed.)

### Ring-tab dropdown — a real native `Combobox` (`Rcp_RingGraphic`)
The Ring-tab picker is a native **`CComboWnd`** (same widget Zeal used for this exact feature). Earlier
notes warned a runtime-instantiated `TabBox` doesn't route input — but that's specific to the
page-managing TabBox; the plain Button/Slider/Label children of our runtime-instantiated `RcpOptions`
already route input fine, and a combobox is self-contained (it owns its popup `CListWnd` at
`+0x1d8`). The combo is driven **entirely by polling**, exactly like the sliders/checkboxes — no
`WndNotification` subclass needed, since our screen uses the stock `CSidlScreenWnd` vtable:
`populate_graphic_combo()` (on create + every open) does `CComboWnd::DeleteAll (0x86A960)` then
`InsertChoice (0x86AE50)` for `get_available_graphics()` ("None" + each `.tga` stem), caches that
index→name list in `graphic_choices_`, and `SetChoice (0x86A740)` to the persisted graphic;
`on_frame` polls `GetCurChoice (0x86A780)` and, on a changed index, maps it back through
`graphic_choices_` and calls `set_graphic`. SIDL is the stock combobox schema (`WDT_Inner` /
`BDT_Combo` / `Style_Border` / `ListHeight` / `Choices`, matching `BUGW_BugTypes`); `InsertChoice`
takes a `const CXStr&` so we build one with `cxstr_init` and release it with `kCXStrDtor` (same as
`set_label_text`). Offsets verified against the locally-cloned eqlib whose
`CreateXWnd/GetChildItem/slider/color-picker` addresses all match this build. The window grew
`356 → 400` tall so the drop list (bottom ~388) sits inside the window.

### Options window — new "Ring" tab (tab index 5)
Tab strip grew 5→6 (window widened 356→**424** to keep the 64 px tab width; tab-5 right edge 411).
Controls (ScreenIDs `Rcp_Ring*`): enable checkbox, **hide-under-self** checkbox, a **color swatch**
button (its text tinted the live ring color; click opens the stock `CColorPickerWnd`), and outer /
inner / opacity sliders with value labels. The ring color is the ring's **own** setting (not a
nameplate role): the picker reuses the existing machinery via a sentinel `picker_role_ ==
kRingColorRole (100)` that `open_color_picker` + the picker-poll branch on. `kTabCount` 5→6 in
`rcp_options_ui.h`; the rest is the standard bind/sync/poll/show-hide pattern. Radius sliders map
0..60 → 0..`radius_max()`(30) world units; opacity 0..100 → 0..1. Regenerate order unchanged:
`gen_option_overrides.py` then `gen_rcp_options_ui.py`.

Caveat (inherited from Zeal): the ring is a **flat disc** at the floor Z under the target, so on
steep slopes / stairs it clips into or floats over the terrain — Zeal tried per-vertex
world-collision height and abandoned it as worse. Built + installed; floor-Z placement + con-color
toggle added 2026-07-09; **optional ring graphic (texture) added 2026-07-09** — awaiting in-game
confirm.

## View distance / far clip (`src/view_distance.cpp`, `/rcpviewdist`) — DONE (confirmed in-game 2026-07-09)

Goal: extend the world/terrain view distance far past the stock "Clip Plane" slider cap, up to
seeing the whole zone. Fog (the soft view wall) was already removed by `no_fog.cpp`; this raises the
hard geometry far clip.

### Key finding — the stock "ClipPlane" integer is NOT a live knob (it mostly drives fog)
The options "Clip Plane" slider maps to an int at `ds:0x00DE0C00` (ini `[Options] ClipPlane`, min 20,
`FogScale @ 0x00DE0D34 = ClipPlane*0.2` clamped [0.1,4.0]). **Poking that int does nothing visible** —
it feeds fog (the routine at `0x48afa7` writes fog RGB bytes to `[cam+0x140..142]`) and an
event-driven camera path, but the projection is never rebuilt from it per frame. The real far clip
lives on the **render camera**: `*(void**)0x00DD2660` is the camera/view object, **`camera+0x8`**
(float) is the far clip distance, handed to the graphics-engine camera sub-object `*(camera+0x118)`
via its **vtbl `+0x0c`** when the projection is rebuilt by `0x0048F290` (which also sets
near=`camera+0x2900`→vtbl `+0x04` and FOV via `fptan`/`fpatan`; the giveaway is the nearby string
`"Camera Aspect Ratio is now %f. (NORM = 1.25. Use 10000 to reset)"`). The setter
**`0x004940A0(camera, float dist)`** does `camera+0x8 = dist; call 0x48F290(camera)` — exactly what the
slider calls. `camera+0x8` is written by only 4 event sites (slider `0x709c95`, settings-apply
`0x4d87d9`/`0x4d88b6`, zone-load `0x53ac1d`), **never per frame**, which is why a source poke never
rebuilds. There is no upper clamp in this path; distant terrain IS submitted (no BSP/LOD wall) and no
engine far-plane cap was hit at 12000 units.

### Mechanism — call the client's own camera-clip setter, re-assert on drift
`view_distance.cpp` registers a `directx::add_render_callback` (EndScene). When enabled it does a
drift check — if `fabs(*(float*)(camera+0x8) - target) > 0.5` it calls `0x004940A0(camera, target)` to
set the far clip and rebuild the projection. Normally that's a single float compare with no client
call; the drift path self-heals after a zone reset (which restores the stock distance via `0x53ac1d`).
Guards: `camera = *(void**)0xDD2660` non-null AND `*(camera+0x118)` non-null (0x48F290 dereferences
`+0x118` without a null check), so it's a no-op off-scene (char select / loading). It captures the
stock `camera+0x8` once so `/rcpviewdist off` restores it.

`/rcpviewdist <n>` forces the far clip to **n world units** (higher = farther; direct distance, not
the slider's lerp), `/rcpviewdist off` restores the client default, bare `/rcpviewdist` prints status
including the **live far clip** (read the stock baseline, then set above it). Persisted to
`[ViewDistance] FarClip`/`ActorClip` in `rof2ClientPlus.ini`; default OFF. **Confirmed in-game
2026-07-09: `/rcpviewdist 12000` renders the whole zone.**

### Actors — `/rcpactordist` (confirmed in-game 2026-07-09)
Actors (NPCs/players) use a **separate** field on the same camera object: **`camera+0x2d78`** (int),
whose engine distance is `int*10 + 50` (stock `0x32`=50 → 550 world units, why mobs pop in close). It
is fed to the graphics camera via vtbl `+0x14` during the same `0x48F290` rebuild (load ~`0x491e69`
"ActorClipPlane" → `[edi+0x2d78]`, save ~`0x615662`). `view_distance.cpp` drives terrain + actors
through one `apply()`+rebuild; `/rcpactordist <n>` takes world units and converts `int=(n-50)/10`.
**Shadows are `camera+0x2d7c` (vtbl `+0x1c`) — deliberately never touched.**

### Display-tab UI (two sliders, done)
Added to the `/rcpoptions` Display tab (NOT the stock options sliders): **Rcp_FarClip** (terrain) and
**Rcp_ActorClip** (actors), each 0..200 steps → 0..20000 world units (100/step; raw 0 = OFF), with a
"off"/value label. Standard bind/sync/poll/show-hide pattern (mirrors `Rcp_NpDist`); the poll calls
`view_distance_settings::set_clip` / `set_actor_clip`. Regenerate order unchanged:
`gen_option_overrides.py` then `gen_rcp_options_ui.py` (81 controls now). Full RE trail in the
`view-distance-re` memory.

---

## GOTCHA: the vendored `game_functions` / `game_ui` / `game_structures` layer is STALE TAKP
`src/game_functions.cpp`, `src/game_ui.h`, `src/game_structures.h`, `src/game_addresses.h` were vendored
verbatim from Zeal and **still hold TAKP (Mac-client) addresses/offsets — they are NOT repointed to our
RoF2 `eqgame.exe`.** They are byte-identical to Zeal's originals (e.g. `can_use_item`@0x4BB8E8,
`can_item_equip_in_slot`@0x4F0DB4, `get_char_info`@0x7F94E8, `InvSlot::HandleLButtonUp`@0x421E48,
`CInvSlotMgr::FindInvSlot`@0x423010, `InvSlotWnd::HandleRButtonUp`@0x5a7e80). Disassembling any of them in
our binary shows mid-function/garbage, not a clean prologue — e.g. 0x5a7e80 is the inner loop of a
vector-copy routine at 0x5a7e50, so hooking it corrupts code.

**Every WORKING RoF2 feature (nameplate, target_ring, view_distance, no_fog, camera) is self-contained
with its own independently-RE'd raw RoF2 addresses and deliberately avoids this vendored layer.** Treat
the vendored `Rcp::Game::*` inventory/item/character helpers and the `InvSlot`/`InvSlotWnd`/`CInvSlotMgr`
structs as UNTRUSTWORTHY for RoF2 until each address is re-verified by disasm. eqlib
(`/home/joshua/workspace/GitHub/eqlib`) is a good RoF2 struct-SHAPE reference, but its addresses target a
DIFFERENT RoF2 build (e.g. `CInvSlot__HandleRButtonUp_x=0x697250`) and do NOT match ours — re-find every
address in our `eqgame.exe`.

### The eqlib rescue: eqlib's RoF2 build == ours
The first `/rcpequip` attempt was built on the vendored TAKP layer above and did nothing (all addresses
wrong; the InvSlotWnd RButton "hook" even patched mid-function code). It was reverted, then **rebuilt
RoF2-native** — and the key discovery that made it easy: **the eqlib fact-reference
(`/home/joshua/workspace/GitHub/eqlib`) is compiled for our EXACT build.** Its own comment reads
`@sizeof(CInvSlotMgr) == 0x2014 :: 2013-05-10` and every offset we spot-checked
(`CInvSlot::HandleRButtonUp` 0x697250, `CInvSlotWnd::HandleLButtonUp` 0x69A5D0, `pinstLocalPlayer`
0xDD2630 == target_ring's `kSelf`) lands on a clean function prologue / correct global in our
`eqgame.exe`. So eqlib's `offsets/eqgame.h` addresses AND struct offsets are directly usable. (eqlib
still targets a slightly different *patch* than some tools expect, so always confirm an address is a
clean prologue before trusting it - but for the inventory system it was 1:1.)

## Right-click to equip — `/rcpequip` (RoF2-native, DONE + confirmed in-game 2026-07-09)
Lives in `src/equip_item.cpp`, fully self-contained with stock RoF2 addresses (eqlib-sourced, disasm-
confirmed) - it does NOT touch the vendored TAKP layer. Ships OFF; `/rcpequip [on|off]` + a
"Right-click to equip" checkbox on the `/rcpoptions` **Mouse** tab (`Rcp_Equip`). Persisted to
`[EquipItem] RightClickToEquip`.

### Behavior
- **Right-click** a wearable item in a bag -> auto-equips into the first slot (priority order:
  weapons/range, armor, jewelry, charm/power-source, ammo) its `EquipSlots` bitmask allows - UNLESS the
  item is a **clicky**, in which case it falls through and the client casts it (which is what a plain
  right-click does natively). So clickies cast, plain gear equips.
- **Alt + right-click** a wearable item -> always equips (so a clicky can still be equipped).
- Everything else (the bag itself, non-wearables, worn items) falls through to the client's native
  right-click unchanged. Additive: with the feature off, or on the fall-through paths, behavior is stock.
- "Clicky" = `ItemDefinition.SpellData@0x284 -> Spells[ItemSpellType_Clicky=0].SpellID (@+0x00) > 0`
  (the first spell slot also covers mount/illusion/familiar/blessing - all right-click activatable).

### Mechanism
Detour on **`CInvSlot::HandleRButtonUp` @0x697250** (thiscall `(CInvSlot*, const CXPoint&)`; the seam
the client itself dispatches right-clicks to). Per-click:
- Source `ItemGlobalIndex` is read from `this->pInvSlotWnd@0x4 -> ItemLocation@0x264`
  (`{ int Location; short slots[3]; }`, 0x0c bytes; Possessions==0, worn 0..22, bag top-slots 23..32).
- The item is fetched via **`CInvSlot::GetItem` @0x694780** (returns an 8-byte `ItemPtr` by hidden
  return pointer; `ItemPtr[0]` is the raw `ItemClient*`). **The definition is at
  `ItemClient::SharedItemDef@0x144`** (a `SharedPtr<ItemDefinition>` in the DERIVED class - NOT
  `ItemBase::Item1@0x9c`, which is null on `ItemClient`; this cost a debugging round-trip).
  `EquipSlots` is `ItemDefinition@0xf0` (bitmask `1<<eInventorySlot`).
- Equip drives the client's own **`CInvSlotMgr::MoveItem` @0x698D80**
  (`pinstCInvSlotMgr` @0xD1FD80; args are two `ItemGlobalIndex*` + 4 bools) - it does the whole
  move/swap on indices, no `ItemClient*` needed.
- **Modifier read:** the Alt state comes from the manager's cached keyboard-flag bytes
  (`pinstCXWndManager` @0x15D3D00, `LAlt@0x9f`/`RAlt@0xa0`; these are what `GetKeyboardFlags@0x875BD0`
  reads). Plain right-click passes `skip_clicky=true` to the equip path (a clicky then falls through and
  the client's own no-Alt handler casts it); Alt passes `skip_clicky=false` (always equip). No flag
  patching / packet RE - clicky casting is just the client's native right-click that we decline to
  absorb. (An earlier version mapped it the other way and used an Alt-flag-clearing trick to force the
  cast; the current scheme is simpler because native plain-right-click already casts.)

### Reference counting
`GetItem` returns an `ItemPtr` (`VePointer<ItemClient>`, an INTRUSIVE smart pointer) by value, so the
copy bumps the object's refcount. We grab the raw pointer and immediately `InterlockedDecrement` it back
(the refcount is `VeBaseReferenceCount::ReferenceCount` @ item+0x4 - `VeBaseReferenceCount` is
`ItemBase`'s first base: `{ vtable@0x0; int ReferenceCount@0x4 }`). The inventory keeps its own
reference, so the count never hits 0 during our synchronous handler and the item stays valid; this just
balances the copy so there's no per-equip leak.

### Dropped vs Zeal
Shift/Ctrl "pick the Nth valid slot" and the empty-slot-first preference (v1 is first-fit; MoveItem
swaps if the target slot is occupied); the separate `ClickFromInventory`/`UseAltForClicky` settings
(one `/rcpequip` toggle gates both behaviors); the bard-melody queue (no melody module here).

---

## Chat shortcuts (Zeal `chat.cpp` percent-replacements → RoF2) — DONE (awaiting in-game confirm 2026-07-09)

`src/chat_shortcuts.cpp`. Always on (no command/toggle - the expansion is harmless when a token isn't
present). Expands short tokens typed into any OUTGOING chat line into live values, exactly like Zeal's
`DoPercentReplacements`:

- `%n`   → current mana percent (`"73%"`)
- `%h`   → current hit-point percent
- `%loc` → your location, `Y, X, Z` (client `/loc` order, 2 decimals)
- `%thp` → your target's HP percent

### Render/hook seam
Detour on **`CEverQuest::DoPercentConvert(char* line, bool bOutGoing)` @ `0x51B600`** (`__thiscall`,
`this` = the CEverQuest instance `*(0xE67CCC)` = `pinstCEverQuest`). This is the stock client function
that already expands `%t`/`%s`/… on outgoing chat, so hooking it is the natural seam. We substitute our
tokens FIRST (plain literal replace-all — the four tokens share no prefix and no replacement value
introduces a token, so order is irrelevant), then tail-call the original for the client's own tokens.

- **Confirmed offset (disasm + eqlib):** `0x51B600` starts `81 ec 0c 08 00 00` (`sub $0x80c,%esp`)
  then `push $0x25` (`'%'`) + `call 0x8dd880` (strchr) — the DoPercentConvert fingerprint. 18 callers;
  outgoing chat (say/tell/group/guild, the `0x51f4xx..0x51f8xx` dispatch cluster near InterpretCmd
  `0x51fce0`) passes `bOutGoing = 1`; the incoming/display path passes `0`. We gate on `bOutGoing` so an
  inbound message that literally contains `%h` is never rewritten.
- **⚠ STALE-OFFSET FIX:** `game_functions.h` (`GameInternal::DoPercentConvert`) and
  `game_structures.h` (`GameClass::DoPercentConvert`) both carried **`0x538110`**, a TAKP offset that
  lands **mid-function** in RoF2 (`0x538110` disassembles to `jne 0x538148`, not a prologue). It was
  dead code so nobody had hit it. Both are now corrected to `0x51B600` (would have crashed if ever
  called). Cross-checked: neighbours `dsp_chat 0x51F1A0` and `InterpretCmd 0x51FCE0` already match eqlib.

### Values
- HP%/mana% via the same client accessors the billboard nameplates use (`self_stats` in
  `font_overlay.cpp`): `pLocalPC (*(0xDD261C)) + 0x2DC8` = CharacterZoneClient `this`, then
  `Cur_HP 0x449E00 / Max_HP 0x443FA0 / Cur_Mana 0x4442E0 / Max_Mana 0x581E60`. -1 (no mana pool /
  no target) renders as `0%`.
- Loc from the local player's position floats `Y@0x64 / X@0x68 / Z@0x6c` (eqlib PlayerBase; EQ memory
  order is Y,X,Z, which is why the printed order matches `/loc`).
- Target HP% via `HpCurrent@0x2e4 / HpMax@0x2dc` (NPCs carry a bare percent in Current with Max 0 —
  same `hp_percent` fallback as nameplate.cpp).
- **⚠ Self/target pointers must be the raw eqlib globals, NOT `Rcp::Game::get_self()`/`get_target()`.**
  Those helpers dereference the STALE-TAKP `game_addresses.h` globals (`Self 0x7F94CC`, `Target
  0x7F94EC`) which are null on RoF2 — the first cut read `0` for `%loc`/`%thp` because of exactly this.
  Use the confirmed globals `pinstLocalPlayer *(0xDD2630)` and `pinstTarget *(0xDD2648)` directly (as
  target_ring.cpp / nameplate.cpp do). `%n`/`%h` were unaffected because they already read
  `pinstLocalPC *(0xDD261C)`.

### Safety
Game-memory reads happen inside `rcp_guard::run`; the `std::string` build/replace runs AFTER the guard
(heap-only, can't fault). Write-back is bounded to 2000 bytes, comfortably under the client's own
`0x800` working buffer that every DoPercentConvert caller must already provide.

---

## Disable sounds by name (`src/sound_mods.cpp`, `/rcpsound`) — DONE (awaiting in-game confirm)

Goal: an in-game way to mute specific game sounds, starting with the storm **thunder** (`thunder1.wav`
/ `thunder2.wav`, packaged in `snd2.pfs`), via a **"Sounds" tab** in `/rcpoptions`, structured so more
sound types can be added later. Toggling mutes/unmutes **instantly, both directions** (no zoning).

### Key finding — RoF2 sound architecture (all disasm-verified against the mss32.dll call graph)
The exe **never names individual wavs** (thunder1/2.wav come from the packaged data, not code — a
`strings eqgame.exe | grep thunder` only hits zone names). Sounds are loaded by filename into an
**audio-asset object**, then played by index, so name-based filtering must happen where the asset is
known. The RE that pinned the seam:
- **Audio-asset ctor `0x5BE830`** (`__thiscall`, `ret 0x14` = 5 args): copies the source filename into
  an **inline buffer at `asset+0x8`**, detects the extension (`strrchr('.')`) and stores a **format at
  `asset+0x210`** (1=`.wav`, 2=`.mp3`, 3=`.ogg`, 4=`.xmi`), plus the file bytes `@+0x208` and size
  `@+0x20c`. So every asset retains its own filename.
- **The single play choke is `Asset::Play` @ `0x5BFBD0`** — `__thiscall(this=asset, void* param)`,
  `ret 4`. It reads `this+0x210`/`+0x214`, dispatches 2D/3D/stream by format, and does the positional
  attenuation, so **every** sound start funnels through it. Its main caller, the "play sound at index N"
  helper `0x4A9140`, is called from all over the binary (combat, spells, UI, weather). The function's
  own null/early-outs `xor eax,eax; ret 4`, so **returning 0 without calling the original cleanly
  suppresses a sound** — a "did not play" result callers already handle.
- Miles (`mss32.dll`) sits one layer below and is **name-blind**: 2D loads via
  `AIL_set_named_sample_file` pass only a **format tag** string (`.mp3`/`.wav`/`.ogg` @ `0x9ca298`/
  `0x9ca308`/`0x9d93dc`), not the wav name; 3D uses `AIL_set_3D_sample_file` (image only). The IAT lives
  at VA `0x9C04D4` (= winedump import offset + image base `0x400000`); the load/play wrappers cluster at
  `0x5BE000..0x5BFA00`. So hooking Miles can't filter by name — the asset layer is the right seam.

### Mechanism — one detour on `Asset::Play`, live name check (mute) + gain scale (volume)
`sound_mods.cpp` detours `0x5BFBD0`. Every play, it reads `(char*)(this+0x8)` **inside `rcp_guard::run`**
(a raw read that could fault on a stale asset), then does all string/map work AFTER the guard (heap-only,
can't fault — same split as `chat_shortcuts.cpp`). Two levers, both **per-play against the live setting**
so they take effect **instantly, both directions, with no data destruction and no zoning** — no need to
locate the SoundManager instance or sweep loaded assets (both considered and rejected as more surface):
- **Mute** → `return 0` (skip), the value the function's own null/early-outs already produce.
- **Volume** → the play request `param`'s first float (`param+0x0`) is the **requested gain** (Play
  early-outs silently when it is ≤ 0), so we transiently `*param *= pct/100`, call the original, then
  **restore** `*param` (the caller's struct is left untouched; Play copies the gain into its queued
  instance synchronously, so restoring after it returns is safe). `param == null` (some default-volume
  2D sounds) → volume can't scale, but mute still works.

**CONFIRMED in-game 2026-07-09** (thunder muting). The `param+0x0` = linear-gain assumption still needs
an in-game check for the volume path (the ≤0→silent gate strongly implies it); mute is proven.

### History + per-sound overrides (dynamic management)
The hook also records a bounded **history** (`std::map<stem, {display, count, last_tick, vol_pct}>`,
`GetTickCount` for recency, capped 512, guarded by a `std::mutex` since the play hook and the chat
commands both touch it). `vol_pct`: −1 = no override (100%), 0 = mute, 1..100 = volume. Preset **named
categories** (`SoundCategory`, seeded `thunder → {thunder1.wav, thunder2.wav}`) remain for the tab
checkbox; the play hook mutes if the stem is in an enabled category OR its `vol_pct == 0`, and scales if
`0 < vol_pct < 100`. All matching is on the **basename stem** (lowercased, path/ext-insensitive).

Commands (`/rcpsound`): `recent [N]` lists the newest-first played sounds numbered (with count + %/MUTED
state, caching index→stem for `#` refs); `vol <name|#> <0-100>` sets a sound's volume (0 = mute; a
`<name>` is stem-matched so it works for not-yet-played sounds, a `<#>` indexes the last `recent` list);
`reset <name|#>` stops tracking a sound; `thunder on|off` (category); `log on|off` (file log of distinct
names); `clear` (wipe history, keep overrides); bare = status. (`mute`/`unmute` were dropped per user —
`vol 0` mutes.) Per-sound overrides persist to a dedicated **`[SoundOverrides]`** section (key = stem,
value = percent; `deleteKey` on reset) via two new `IO_ini` helpers (`getSection`, `deleteKey`);
categories/log stay in `[Sounds]`.

### Sounds tab — the tracked-sound manager (DONE)
The **"Sounds" tab** (tab index 6; tab strip grew 6→7, window widened 424→**492**, tab-6 right edge 478)
is a curated **tracked-sound list** (the preset "Disable thunder" checkbox was removed 2026-07-09 —
redundant once thunder can be muted from the list like any other sound; the `/rcpsound thunder` command +
category backend were kept, and the remaining controls shifted up ~28px, 100 controls): an **"Add a
played sound" combobox** (populated from `sound_settings::get_recent_untracked(30)` — recently-played sounds not yet
tracked), up to **`kSndRowCount`(10) row buttons** (each shows `name    NN%`/`MUTED`; click selects it,
radio-style like the tab strip), a **volume slider** (0..100, 0 = mute) + value label bound to the
selected row, and a **"Remove selected from list"** button. All poll-driven (no `WndNotification`),
mirroring the Colors-tab role buttons + Ring-tab combobox. `refresh_sound_list()` repaints rows only when
a signature of the list changes (so it doesn't fight the user or spam `SetWindowText`); a
`snd_selection_dirty_` flag snaps the slider to the selection's volume on a select/add/remove, and the
slider is left alone mid-drag (its `set_volume` updates the row text live). Adding a sound selects it;
removing it clears the selection and returns it to the add combo. `sound_settings` exposes
`get_tracked` / `get_recent_untracked` / `add_tracked` (returns the stem) / `set_volume` /
`remove_tracked`. Regen order unchanged (`gen_option_overrides.py` then `gen_rcp_options_ui.py`, **101
controls**).

**In-game verification (user):** open the Sounds tab, pick a recently-played sound from the combobox → it
appears as a row; click the row and drag the volume slider (row text updates live); "Remove selected"
drops it. Commands `/rcpsound recent`, `vol 3 50`, `reset 3` do the same. If `vol` doesn't audibly change
a sound (only 0/mute does), `param+0x0` is a gate not the output gain and the volume lever needs re-RE
(the queued-instance gain field / the `AIL_set_sample_volume_levels @0x9c053c` path). Built + installed
2026-07-09.

**Bugfix (2026-07-09):** persisted overrides loaded fine (verified `GetPrivateProfileSectionA` + the
`[SoundOverrides]` read under Wine) but the tracked list rendered EMPTY on window open until the user
interacted. Cause: `refresh_sound_list`'s content-signature guard skipped the repaint on tab entry while
`set_active_tab` had hidden the rows on other tabs, so an unchanged list never re-`show_window`'d them on
return. Fix: `set_active_tab` clears `snd_list_sig_` and always calls `refresh_sound_list` (forcing a
repaint on any tab change), and each row's `show_window` is gated on `active_tab_ == 6`.

**Scrollable-list upgrade (2026-07-09):** the 10 fixed row buttons were replaced by a native **`CListWnd`**
(`Rcp_SndList`) so the tracked list scrolls to any count. SIDL is the stock `<Listbox>` schema (one
`<Columns><Width>` + `Style_VScroll`, modeled on `FW_FriendsList`). CListWnd methods (eqlib +
disasm-verified against our build — `GetCurSel@0x853430` reads `CurSel@this+0x1f8` vs row count
`ItemsArray.m_length@this+0x1d8`): `AddString@0x8580C0`, `GetCurSel@0x853430`, `SetCurSel@0x853470`,
`SetItemText@0x8575B0`, `RemoveLine@0x8582A0`; row count `*(int*)(list+0x1d8)`. Thin wrappers in
`rcp_options_ui.cpp` (all gated on non-null). `refresh_sound_list` now rebuilds the list only on a
**structural** change (the stem set differs); a volume-only edit updates just the changed row via
`SetItemText`, preserving scroll position + selection. Row selection is the list's own polled `GetCurSel`
(no more checkbox rows). This is the first standalone `CListWnd` in the mod — **awaiting in-game verify**
that a runtime-instantiated list renders, routes row clicks, and scrolls.

---

### Floating combat damage (`/rcpfcd`, `src/floating_damage.{h,cpp}`) - Zeal floating_damage port

Rising/fading damage numbers over each entity as it is struck (the classic Zeal "floating combat
text"). Off by default; `/rcpfcd` toggles + subcommands, and a new `/rcpoptions` **"Combat" tab**.
Persisted to `[FloatingDamage]`.

- **Damage source = detour on `CEverQuest::ReportSuccessfulHit` @ `0x52EE40`** (eqlib, which is
  compiled for our exact `__ClientDate 20130510` build), `bool ReportSuccessfulHit(EQSuccessfulHit*,
  bool output_text, int)` - a `__thiscall`; detoured with the standard `__fastcall(this, edx, ...)`
  idiom. This is the same function Zeal hooks on TAKP (Zeal's `0x5297D2`). **The repo's
  `callbacks.cpp` already had a copy of Zeal's `ReportSuccessfulHit` but with the STALE TAKP offset
  `0x5297D2` and the TAKP `Damage_Struct` layout, inside the dead/uninstantiated `CallbackManager` -
  do NOT use it.** The correct RoF2 struct is eqlib's **`EQSuccessfulHit`** (packed): `DamagedID`
  u16@0, `AttackerID` u16@2, `Skill` u8@4, `SpellID` i32@5, `DamageCaused` i32@9 - field sizes differ
  from the TAKP `Damage_Struct` (there `type`/`spellid`/`damage` are all 16-bit).
- **Rendering = 3-D billboard (`SpriteFont`) at the shared pre-UI seam** (`font_overlay::add_scene_draw`),
  reusing the in-game-confirmed nameplate path + the `arial_bold_24` atlas. Head anchor is the model's
  name-sprite world pos (`actor@0x101c` -> `CStringSprite@0x204` -> pos@`+0x6c/0x70/0x74`), fallback
  feet+`AvatarHeight@0x138` - identical to `on_render_nameplates`. Numbers rise in world-Z + fade over
  ~2.2 s (big hits 3 s, larger, own color). Scale is a per-flush font setting, so normal vs big hits
  are bucketed into two flushes.
- **No risky offsets in the hot path.** Categorization (mine / to-me / other) is by comparing the
  resolved source/target pointers to the `self`/`controlled` globals (`0xDD2630`/`0xDD2644`); pet
  detection reads eqlib `SpawnID@0x148` / `MasterID@0x38c` (same PlayerBase struct as the confirmed
  pos/heading offsets) but only inside the crash guard, and a wrong read only mis-colors. Spawn-id ->
  entity via `Rcp::Game::get_entity_by_id` (`EntityIdArray@0x0078c47c`, size 5000; `pinstSpawnManager
  0xE641D0` matches eqlib, so the array base is trusted). **Note: `game_structures.h`'s `Entity` struct
  (SpawnId@0x94, Type@0xa8, Position@0x48) is the STALE TAKP layout - do NOT read spawn fields through
  it; use the raw eqlib offsets like target_ring/font_overlay do.**
- **Threading:** the hit detour fires on the main thread, the draw on the render thread. A `std::mutex`
  guards the active-number list; the detour builds primitives under the crash guard (no lock/dtor
  inside the guard, so a longjmp can't strand the mutex), then pushes under the lock OUTSIDE the guard;
  the draw snapshots+expires under the lock, then renders without it.
- **Options "Combat" tab** (9th tab; window widened to `CX=624`): enable + 5 filter checkboxes
  (mine/incoming/others/melee/spells) + big-hit threshold slider (0..2000) + 4 color swatches
  (sentinel picker roles `101..104`, mirroring the ring's `100`). Kept to ONE general tab per user
  request (avoid granular tabs).
- **CONFIRMED in-game 2026-07-09.** Tuning constants (`kScaleNormal/Big`, `kBaseLift`, `kRiseWorld`,
  `kJitter`, durations) are at the top of the .cpp if the size/height/scatter want adjusting.
- **First-test bug (fixed):** nothing appeared because the spawn-id -> entity lookup used the vendored
  `Rcp::Game::get_entity_by_id` (flat `EntityIdArray@0x0078c47c`), a **STALE TAKP address on RoF2** that
  returns a garbage pointer - reading a field off it faulted every hit (the crash guard swallowed
  `ACCESS_VIOLATION in 'fcd.classify'`, so it failed silently, no numbers). Disasm of ReportSuccessfulHit
  itself (`mov ecx,[0xE641D0]; push id; call 0x5996E0`) + eqlib gave the real lookup:
  **`PlayerManagerClient::GetSpawnByID(int) @ 0x5996E0`, this = pinstSpawnManager `*(void**)0xE641D0`,
  `__thiscall`** - the module now uses a local `get_spawn_by_id()`. The detour ABI and the
  `EQSuccessfulHit` field reads were correct all along; only the lookup was stale. **General lesson:
  `get_entity_by_id` is unreliable on RoF2 - prefer `GetSpawnByID@0x5996E0`.**

## Extra /hidecorpses options: always + showlast (`src/hide_corpse.{h,cpp}`) — DONE (awaiting in-game confirm)

**Extends the stock `/hidecorpses` command** (does NOT add a separate command) with the two conveniences
RoF2 native lacks: `/hidecorpses always` (keep every NPC corpse hidden, including ones that spawn after
the command) and `/hidecorpses showlast` (reveal the single most-recently-hidden corpse so you can loot
it). Player corpses (incl. your own) are never hidden. The command hook intercepts `/hidecorpses`, handles
`always`/`showlast`/`debug` (returns true), and **returns false for the native keywords + no-arg so the
stock handler still runs** (case-insensitive; `none` also cancels `always` so the scan stops re-hiding).

### Native `/hidecorpses` is plural, one-shot, and server-mediated (so both features are genuinely new)
- The native command is **`/hidecorpses`** (plural, handler `0x4E78C0`), not `/hidecorpse`. Its option
  keywords are string-DB resources (not ASCII in the exe — that's why they don't grep): id `0x3395`=ALL,
  `0x3397`=ALLBUTGROUP, `0x33F6`=LOOTED, `0x18D6`=NPC, plus NONE. Usage text (id 0x3392/0x3393) states
  *"any corpses created after use of this command will not be hidden"* and *"your own corpse(s) will
  never be hidden."* Every mode just enqueues the mode to the server via `0x8C51F0` (a mutex-guarded
  message queue on the connection object `0xDD25AC`) + sets a LOOTED byte at `camera+0x14` (camera =
  `*(void**)0xDD2660`). There is **no continuous mode, no showlast, and no callable local "hide all
  spawns" function** — so we add those two keywords to the command itself and forward the native ones.

### Model-hide primitive — reuse the client's own `/hideme` field (do NOT reinvent)
- **Write `spawn+0x338` (int32): `3001` (0xBB9) = invisible, `0` = visible.** This is exactly what the
  GM `/hideme` command (handler `0x4FBB40`) toggles. The spawn vtable setter `[+0xD8] = 0x597F30` is
  literally `mov [ecx+0x338], arg; ret 4`, so a **direct field write is identical to the client's own
  path and avoids any indirect-call risk** (hide_model/show_model write the field directly, only when it
  differs, to avoid re-triggering the refresh each frame). eqstr confirms semantics: id 0x3296 "%1 is now
  Invisible.", 0x3297 "%1 is now Visible." The client's per-frame spawn processing `0x4592B0` reads this
  field and calls actor-refresh `0x434090` — so the render loop honours it; our EndScene scan re-asserts
  it every frame, which keeps a corpse hidden even if the client consumes/clears the field between frames.
  A name-sprite reader at `0x58E822` also treats `[spawn+0x338]>0` as invisible. **Note the offset aliases
  to a POINTER on a different object class (`0x6C93B9`/`0x70222E`) — only write it on spawns walked from
  the PlayerManagerClient list, which are PlayerClient-layout.** If a set alone ever fails to re-cull the
  3D model in-game, add `reinterpret_cast<void(__thiscall*)(void*)>(0x434090)(spawn)` after the write.

### NPC-vs-PC corpse — no clean field; name-shape heuristic (protects your own corpse)
- `type@0x125` ∈ {0=Player,1=NPC,2=Corpse} only (verified: **no value 3 anywhere**; `IsCorpse` at
  `0x8CFCD0` = `cmp [ecx+0x125],2`), so it can't tell an NPC corpse from a player corpse — that split is
  server-side in native `/hidecorpses NPC`. Heuristic: a corpse's display name (`+0xe4`) is
  "<Base>'s corpse"; take the part before the `'`; if <Base> is a single **capitalized all-alpha token**
  it's a PLAYER corpse → never hidden (EQ player names can't contain space/digit/underscore, so this
  always protects your own corpse). Everything else ("a bat", "an orc pawn", "Lord Nagafen",
  "gnoll_pup039") is hidden. Only miss: a single-token capitalized *named* NPC stays visible.
  **`/hidecorpses debug`** dumps the target's name/type/`+0x338`/`+0x150`(fork's candidate discriminator,
  getter `0x8CFCE0`)/spawnid so a clean PC/NPC field can be pinned in-game and replace the heuristic.

### Design + shared offsets
- Spawn walk (confirmed, shared w/ font_overlay): mgr `*(void**)0xE641D0` → head `+0x08`, next `+0x08`;
  type `+0x125`; actor(CActorInterface*) `+0x101c`; SpawnID(u32) `+0x148`; name `+0xe4`; self
  `*(void**)0xDD2630`; target `*(void**)0xDD2648`. Show-last validates via `GetSpawnByID@0x5996E0`
  (this=`*0xE641D0`), never the stale vendored lookup.
- `always` = persisted `[HideCorpse] Always`; a `directx::add_render_callback` (EndScene) scan hides
  every non-revealed NPC corpse and records the most-recently-newly-hidden as "last". `showlast` un-hides
  that one and adds it to a `revealed` map (validated by id→ptr) so the scan won't re-hide it; toggling
  `always` off reveals everything we hid. `g_writes_armed` is a master kill-switch (ships true because
  the field write is safe).
- **Dead ends (don't retry):** TAKP `ActorInfo->IsInvisible` / `ViewActor->Flags |= 0x40000000` is stale
  on RoF2 (DPVS engine; the DLL's only 0x40000000 tests are CPUID/format false-positives); the
  `Active_Corpse` global `0x7f9500` has **0 xrefs** (stale); native `/hidecorpses` can't be driven
  locally (server round-trip); the CActorInterface `+0x101c` vtable route is 133+ entries with unresolved
  RTTI — the `/hideme` `+0x338` field is simpler and is the client's own path.
