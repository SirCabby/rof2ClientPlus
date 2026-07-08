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
static float g_distance = 0.0f;  // 0 = use the native wheel zoom distance.
static float g_height = 0.0f;    // World-unit Z raise above the native camera height.
static bool g_collision = false;  // Pull the camera in when a wall blocks the view.
static int g_log = 0;            // Calibration log budget (dumps native camera state on enable).
static int g_col_log = 0;        // Collision-pull log budget (verifies the raycast in-game).

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static constexpr char kIniSection[] = "Chase";

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "Enabled")) g_enabled = ini.getValue<bool>(kIniSection, "Enabled");
  if (ini.exists(kIniSection, "Distance")) g_distance = ini.getValue<float>(kIniSection, "Distance");
  if (ini.exists(kIniSection, "Height")) g_height = ini.getValue<float>(kIniSection, "Height");
  if (ini.exists(kIniSection, "Collision")) g_collision = ini.getValue<bool>(kIniSection, "Collision");
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "Enabled", g_enabled);
  ini.setValue<float>(kIniSection, "Distance", g_distance);
  ini.setValue<float>(kIniSection, "Height", g_height);
  ini.setValue<bool>(kIniSection, "Collision", g_collision);
}

namespace chase_settings {
bool get_enabled() { return g_enabled; }
float get_distance() { return g_distance; }
float get_height() { return g_height; }
bool get_collision() { return g_collision; }
void set(bool enabled, float distance, float height, bool collision) {
  if (enabled && !g_enabled) g_log = 30;  // Fresh enable -> capture calibration frames.
  g_enabled = enabled;
  g_distance = clampf(distance, 0.0f, 100.0f);
  g_height = clampf(height, -20.0f, 60.0f);
  g_collision = collision;
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

  const float dd = g_distance > 0.0f ? g_distance : nd;  // Target distance along the native ray.
  const float s = dd / nd;
  float cam_x = px + ox * s;
  float cam_y = py + oy * s;
  float cam_z = nz + g_height;  // native height + optional raise

  // Collision: if world geometry blocks the pivot->camera line, pull the camera in
  // to the clamp point (kept 10% short of the wall so it doesn't clip through).
  if (g_collision) {
    float clamp[3];
    if (collide_world(px, py, nz, cam_x, cam_y, cam_z, clamp)) {
      const float margin = 0.9f;
      if (g_col_log > 0) {
        logger::logf("[chase] collide wanted=(%.1f,%.1f,%.1f) clamp=(%.1f,%.1f,%.1f)", cam_x, cam_y, cam_z, clamp[0],
                     clamp[1], clamp[2]);
        --g_col_log;
      }
      cam_x = px + (clamp[0] - px) * margin;
      cam_y = py + (clamp[1] - py) * margin;
      cam_z = nz + (clamp[2] - nz) * margin;
    }
  }

  *reinterpret_cast<float *>(cam + kCamPosX) = cam_x;
  *reinterpret_cast<float *>(cam + kCamPosY) = cam_y;
  *reinterpret_cast<float *>(cam + kCamPosZ) = cam_z;
  });  // rcp_guard::run("chase.cam6")
}

static void print_status() {
  char dist[24];
  if (g_distance > 0.0f)
    std::snprintf(dist, sizeof(dist), "%.1f", g_distance);
  else
    std::snprintf(dist, sizeof(dist), "native");
  char msg[192];
  std::snprintf(msg, sizeof(msg), "rof2ClientPlus chase cam: %s | distance=%s | height=+%.1f | collision=%s",
                g_enabled ? "ON" : "OFF", dist, g_height, g_collision ? "on" : "off");
  Rcp::Game::print_chat(msg);
}

ChaseCam::ChaseCam(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  // Install the Cam6 positioner detour now. It only acts while the chase view
  // (cameraType 6) is active and we're enabled, so installing at load time is safe.
  rcp->hooks->Add("chase_cam6", static_cast<int>(kCam6Positioner), Cam6Pos_hk, hook_type_detour);
  g_orig_cam6 = rcp->hooks->hook_map["chase_cam6"]->original(Cam6Pos_hk);
  if (g_enabled) g_log = 30;
  logger::logf("[chase] Cam6 positioner hooked at 0x%x (enabled=%d dist=%.1f height=%.1f)", (unsigned)kCam6Positioner,
               (int)g_enabled, g_distance, g_height);

  rcp->commands_hook->Add(
      "/rcpchase", {"/rcpchasecam"},
      "Third-person chase cam. '/rcpchase on|off', '/rcpchase dist <n|native>', '/rcpchase height <n>', "
      "'/rcpchase collision on|off'.",
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
          } else if (args.size() >= 3 && (args[1] == "dist" || args[1] == "distance")) {
            g_distance = (args[2] == "native" || args[2] == "0") ? 0.0f : clampf(std::stof(args[2]), 1.0f, 100.0f);
            if (!g_enabled) g_log = 30;
            g_enabled = true;
          } else if (args.size() >= 3 && args[1] == "height") {
            g_height = clampf(std::stof(args[2]), -20.0f, 60.0f);
            if (!g_enabled) g_log = 30;
            g_enabled = true;
          } else {
            if (!g_enabled) g_log = 30;
            g_enabled = !g_enabled;  // Bare /rcpchase toggles.
          }
        } catch (...) {
          Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpchase on|off | dist <n> | height <n>'");
          return true;
        }
        save_settings();
        print_status();
        return true;
      });
  logger::log("[chase] /rcpchase registered");
}
