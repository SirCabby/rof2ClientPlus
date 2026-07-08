// rof2ClientPlus - third-person chase camera (ZealCam Phase 2), stock RoF2 port.
// See chase_cam.h and PORTING_NOTES.md (Phase 2) for the mechanism + full RE.
#include "chase_cam.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "memory.h"
#include "rcp.h"

// ---- Stock RoF2 addresses (confirmed in the disassembly; see PORTING_NOTES) ----
static int *const kCameraType = reinterpret_cast<int *>(0xD1FD9C);      // CDisplay::cameraType (6 = wheel chase)
static void **const kControlled = reinterpret_cast<void **>(0xDD2644);  // controlled player (self/mount/charm)
static constexpr int kChaseView = 6;  // Cameras[6] @ 0xDE0D7C is the mouse-wheel follow camera in this build.

// Cam6's per-frame positioner (EQCamera vtable 0x9d1188 slot +0x08). The per-frame
// driver 0x796DD0 reads the camera's Position (0x04/0x08/0x0c) and Orientation
// (0x10/0x14/0x18) into the renderer immediately AFTER this returns, so writing
// those fields in the tail lands in the rendered view. __thiscall(EQCamera*, actor).
static constexpr uintptr_t kCam6Positioner = 0x799140;

// PlayerClient offsets (eqlib-sourced, disasm-confirmed). EQ stores position Y,X,Z.
static constexpr int kEntY = 0x64;   // world Y (north)
static constexpr int kEntX = 0x68;   // world X (east)
static constexpr int kEntZ = 0x6c;   // world Z (feet/base)

// EQCamera field offsets (eqlib EQData.h EQCamera). The renderer consumes 0x04/0x08/
// 0x0c (position) and 0x10/0x14/0x18 (orientation); the rest are intermediates.
static constexpr int kCamPosY = 0x04;
static constexpr int kCamPosX = 0x08;
static constexpr int kCamPosZ = 0x0c;
static constexpr int kCamOrientY = 0x10;
static constexpr int kCamOrientX = 0x14;
static constexpr int kCamHeading = 0x28;
static constexpr int kCamPitch = 0x30;
static constexpr int kCamDistance = 0x34;
static constexpr int kCamDirHeading = 0x38;

typedef void(__fastcall *Cam6Pos_t)(void *self, int edx, void *actor);
static Cam6Pos_t g_orig_cam6 = nullptr;

// gfMaxZoomCameraDistance: the .rdata float the wheel handler (0x518A70) clamps its
// zoom accumulator (CEverQuest+0x5EC) against. Native value 53.0 (verified in the
// exe). Patching it raises/lowers how far the wheel can zoom out while leaving the
// wheel itself fully functional - which is exactly what a "max distance" should do.
static constexpr uintptr_t kMaxZoomFloat = 0x9D0DE4;
static constexpr float kNativeMaxZoom = 53.0f;

// ---- World collision (camera pull-in). The client's own line-of-sight primitive:
// build a CCollisionInfoTargetVisibility (ctor 0x8D4570), test it against the
// collision interface (pCollision->vtable[0], nonzero = blocked), then read the
// clamp point (GetCollisionPoint 0x7A2680). All coords are EQ-native Y,X,Z order. ----
static void **const kPCollision = reinterpret_cast<void **>(0x15D46B0);  // SGraphicsEngine::pCollision
static constexpr uintptr_t kCollisionInfoCtor = 0x8D4570;                // __thiscall(info, seg[6], excludeEntity, 0)
static constexpr uintptr_t kGetCollisionPoint = 0x7A2680;                // __thiscall(info, out[3]) -> start+t*dir

// Casts head->wanted through world geometry (mod coords: x=east y=north z=up).
// Returns true if blocked; writes the clamp point (== wanted when clear) to out[3].
static bool collide_world(float fx, float fy, float fz, float tx, float ty, float tz, float out[3]) {
  void *pcol = *kPCollision;
  // The collision interface + its vtable live in eqgraphics.dll (base >= 0x10000000);
  // bail if the graphics engine isn't up yet, so we never call a bad vtable.
  if (!pcol || *reinterpret_cast<uintptr_t *>(pcol) < 0x10000000) {
    out[0] = tx;
    out[1] = ty;
    out[2] = tz;
    return false;
  }
  float seg[6] = {fy, fx, fz, ty - fy, tx - fx, tz - fz};  // {from.Y,from.X,from.Z, dY,dX,dZ}
  char info[0xC0];
  std::memset(info, 0, sizeof(info));
  reinterpret_cast<void(__thiscall *)(void *, void *, void *, int)>(kCollisionInfoCtor)(info, seg, *kControlled, 0);

  void **vtbl = *reinterpret_cast<void ***>(pcol);
  const int blocked = reinterpret_cast<int(__thiscall *)(void *, void *)>(vtbl[0])(pcol, info);

  float raw[3];
  reinterpret_cast<void(__thiscall *)(void *, void *)>(kGetCollisionPoint)(info, raw);
  out[0] = raw[1];  // x = X
  out[1] = raw[0];  // y = Y
  out[2] = raw[2];  // z = Z
  return blocked != 0;
}

// ---- Live settings (persisted to rof2ClientPlus.ini [Chase]). Off by default so
// nothing changes until the user opts in with /rcpchase on, matching mouse_mods. ----
static bool g_enabled = false;
static float g_max_distance = 0.0f;  // Max wheel zoom-out distance; 0 = the native max (53).
static bool g_collision = false;     // Pull the camera in when a wall blocks the view.
static int g_log = 0;            // Calibration log budget (dumps native camera state on enable).
static int g_col_log = 0;        // Collision-pull log budget (verifies the raycast in-game).

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Applies the max-zoom setting to the client: while the feature is enabled with a
// custom max, patch gfMaxZoomCameraDistance so the mouse wheel can zoom out to it;
// otherwise restore the native 53. Idempotent - call after any settings change.
static void apply_max_zoom() {
  const float want = (g_enabled && g_max_distance > 0.0f) ? g_max_distance : kNativeMaxZoom;
  if (*reinterpret_cast<float *>(kMaxZoomFloat) != want) {
    mem::write<float>(static_cast<int>(kMaxZoomFloat), want);
    logger::logf("[chase] gfMaxZoomCameraDistance -> %.1f", want);
  }
}

static constexpr char kIniSection[] = "Chase";

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "Enabled")) g_enabled = ini.getValue<bool>(kIniSection, "Enabled");
  if (ini.exists(kIniSection, "MaxDistance")) g_max_distance = ini.getValue<float>(kIniSection, "MaxDistance");
  if (ini.exists(kIniSection, "Collision")) g_collision = ini.getValue<bool>(kIniSection, "Collision");
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "Enabled", g_enabled);
  ini.setValue<float>(kIniSection, "MaxDistance", g_max_distance);
  ini.setValue<bool>(kIniSection, "Collision", g_collision);
}

namespace chase_settings {
bool get_enabled() { return g_enabled; }
float get_max_distance() { return g_max_distance; }
bool get_collision() { return g_collision; }
void set(bool enabled, float max_distance, bool collision) {
  if (enabled && !g_enabled) g_log = 30;  // Fresh enable -> capture calibration frames.
  g_enabled = enabled;
  g_max_distance = max_distance <= 0.0f ? 0.0f : clampf(max_distance, 10.0f, 300.0f);
  g_collision = collision;
  apply_max_zoom();
  save_settings();
}
}  // namespace chase_settings

// Cam6 positioner tail-hook. The client positions/orients the camera natively first
// (behind the player, centered look, honoring left-click orbit); we then re-derive
// the FINAL position along the client's own camera-behind vector so we can never get
// the "in front" direction wrong, rescaling to our distance and optionally raising it.
// Orientation is left native (it already looks at the player), which keeps the player
// centered while panning. Head-height + independent look come next, once the logged
// calibration nails the orientation units.
static void __fastcall Cam6Pos_hk(void *self, int edx, void *actor) {
  g_orig_cam6(self, edx, actor);  // Native positioning runs first (client code; unguarded).

  if (!g_enabled || *kCameraType != kChaseView) return;
  // Guard OUR positioning: a stale camera/entity pointer, or the collision call into
  // eqgraphics.dll, should skip this frame's adjustment rather than crash the client
  // (the inner early-returns simply exit this lambda). POD body only; see crash_handler.h.
  rcp_guard::run("chase.cam6", [&] {
  void *controlled = *kControlled;
  if (!controlled || !self) return;

  char *p = static_cast<char *>(controlled);
  char *cam = static_cast<char *>(self);

  const float px = *reinterpret_cast<float *>(p + kEntX);
  const float py = *reinterpret_cast<float *>(p + kEntY);
  const float pz = *reinterpret_cast<float *>(p + kEntZ);
  const float nx = *reinterpret_cast<float *>(cam + kCamPosX);
  const float ny = *reinterpret_cast<float *>(cam + kCamPosY);
  const float nz = *reinterpret_cast<float *>(cam + kCamPosZ);

  // The client's own "camera minus player" horizontal vector = the correct behind
  // direction (already reflects heading and any left-click orbit).
  const float ox = nx - px;
  const float oy = ny - py;
  const float nd = std::sqrt(ox * ox + oy * oy);

  if (g_log > 0) {
    logger::logf(
        "[chase] cal player=(%.1f,%.1f,%.1f) natcam=(%.1f,%.1f,%.1f) off=(%.1f,%.1f) nd=%.2f | "
        "camHdg=%.2f camDir=%.2f camPit=%.2f camDist=%.2f orientY=%.2f orientX=%.2f",
        px, py, pz, nx, ny, nz, ox, oy, nd, *reinterpret_cast<float *>(cam + kCamHeading),
        *reinterpret_cast<float *>(cam + kCamDirHeading), *reinterpret_cast<float *>(cam + kCamPitch),
        *reinterpret_cast<float *>(cam + kCamDistance), *reinterpret_cast<float *>(cam + kCamOrientY),
        *reinterpret_cast<float *>(cam + kCamOrientX));
    --g_log;
  }

  if (nd < 0.1f) return;  // Degenerate (camera atop player); leave native.

  // Distance is native: the wheel drives it, with its max raised via the
  // gfMaxZoomCameraDistance patch (apply_max_zoom). Only collision remains here.
  if (!g_collision) return;

  // Collision: if world geometry blocks the pivot->camera line, pull the camera in
  // to the clamp point (kept 10% short of the wall so it doesn't clip through).
  float clamp[3];
  if (collide_world(px, py, nz, nx, ny, nz, clamp)) {
    const float margin = 0.9f;
    if (g_col_log > 0) {
      logger::logf("[chase] collide wanted=(%.1f,%.1f,%.1f) clamp=(%.1f,%.1f,%.1f)", nx, ny, nz, clamp[0], clamp[1],
                   clamp[2]);
      --g_col_log;
    }
    *reinterpret_cast<float *>(cam + kCamPosX) = px + (clamp[0] - px) * margin;
    *reinterpret_cast<float *>(cam + kCamPosY) = py + (clamp[1] - py) * margin;
    *reinterpret_cast<float *>(cam + kCamPosZ) = nz + (clamp[2] - nz) * margin;
  }
  });  // rcp_guard::run("chase.cam6")
}

static void print_status() {
  char dist[24];
  if (g_max_distance > 0.0f)
    std::snprintf(dist, sizeof(dist), "%.0f", g_max_distance);
  else
    std::snprintf(dist, sizeof(dist), "native (53)");
  char msg[192];
  std::snprintf(msg, sizeof(msg), "rof2ClientPlus chase cam: %s | max zoom=%s | collision=%s",
                g_enabled ? "ON" : "OFF", dist, g_collision ? "on" : "off");
  Rcp::Game::print_chat(msg);
}

ChaseCam::ChaseCam(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  // Install the Cam6 positioner detour now. It only acts while the chase view
  // (cameraType 6) is active and we're enabled, so installing at load time is safe.
  rcp->hooks->Add("chase_cam6", static_cast<int>(kCam6Positioner), Cam6Pos_hk, hook_type_detour);
  g_orig_cam6 = rcp->hooks->hook_map["chase_cam6"]->original(Cam6Pos_hk);
  if (g_enabled) g_log = 30;
  apply_max_zoom();  // Re-apply a persisted custom max zoom at load.
  logger::logf("[chase] Cam6 positioner hooked at 0x%x (enabled=%d maxdist=%.1f)", (unsigned)kCam6Positioner,
               (int)g_enabled, g_max_distance);

  rcp->commands_hook->Add(
      "/rcpchase", {"/rcpchasecam"},
      "Third-person chase cam. '/rcpchase on|off', '/rcpchase maxdist <10-300|native>' (how far the wheel can zoom "
      "out), '/rcpchase collision on|off'.",
      [](std::vector<std::string> &args) {
        try {
          if (args.size() >= 2 && (args[1] == "off" || args[1] == "0")) {
            g_enabled = false;
          } else if (args.size() >= 2 && args[1] == "on") {
            if (!g_enabled) g_log = 30;
            g_enabled = true;
          } else if (args.size() >= 3 && (args[1] == "collision" || args[1] == "collide")) {
            g_collision = (args[2] == "on" || args[2] == "1");
            g_col_log = g_collision ? 20 : 0;  // Log the first pull-ins to verify the raycast.
            if (!g_enabled) g_log = 30;
            g_enabled = true;
          } else if (args.size() >= 3 && (args[1] == "maxdist" || args[1] == "dist" || args[1] == "distance")) {
            g_max_distance =
                (args[2] == "native" || args[2] == "0") ? 0.0f : clampf(std::stof(args[2]), 10.0f, 300.0f);
            if (!g_enabled) g_log = 30;
            g_enabled = true;
          } else {
            if (!g_enabled) g_log = 30;
            g_enabled = !g_enabled;  // Bare /rcpchase toggles.
          }
        } catch (...) {
          Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpchase on|off | maxdist <10-300|native> | collision on|off'");
          return true;
        }
        apply_max_zoom();
        save_settings();
        print_status();
        return true;
      });
  logger::log("[chase] /rcpchase registered");
}
