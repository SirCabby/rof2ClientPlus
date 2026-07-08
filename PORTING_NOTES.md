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
