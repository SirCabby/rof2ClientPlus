// rof2ClientPlus - third-person chase camera (ZealCam Phase 2), stock RoF2 port.
// See chase_cam.h and PORTING_NOTES.md (Phase 2) for the mechanism + full RE.
#include "chase_cam.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "commands.h"
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "memory.h"
#include "rcp.h"

// gfMaxZoomCameraDistance: the .rdata float the wheel handler (0x518A70) clamps its
// zoom accumulator (CEverQuest+0x5EC) against. Native value 53.0 (verified in the
// exe). Patching it raises/lowers how far the wheel can zoom out while leaving the
// wheel itself fully functional - which is exactly what a "max distance" should do.
static constexpr uintptr_t kMaxZoomFloat = 0x9D0DE4;
static constexpr float kNativeMaxZoom = 53.0f;

// ---- Live settings (persisted to rof2ClientPlus.ini [Chase]). Off by default so
// nothing changes until the user opts in with /rcpchase on, matching mouse_mods. ----
static bool g_enabled = false;
static float g_max_distance = 0.0f;  // Max wheel zoom-out distance; 0 = the native max (53).

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
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "Enabled", g_enabled);
  ini.setValue<float>(kIniSection, "MaxDistance", g_max_distance);
}

namespace chase_settings {
bool get_enabled() { return g_enabled; }
float get_max_distance() { return g_max_distance; }
void set(bool enabled, float max_distance) {
  g_enabled = enabled;
  g_max_distance = max_distance <= 0.0f ? 0.0f : clampf(max_distance, 10.0f, 300.0f);
  apply_max_zoom();
  save_settings();
}
}  // namespace chase_settings

static void print_status() {
  char dist[24];
  if (g_max_distance > 0.0f)
    std::snprintf(dist, sizeof(dist), "%.0f", g_max_distance);
  else
    std::snprintf(dist, sizeof(dist), "native (53)");
  char msg[192];
  std::snprintf(msg, sizeof(msg), "rof2ClientPlus chase cam: %s | max zoom=%s", g_enabled ? "ON" : "OFF", dist);
  Rcp::Game::print_chat(msg);
}

ChaseCam::ChaseCam(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  apply_max_zoom();  // Re-apply a persisted custom max zoom at load.
  logger::logf("[chase] loaded (enabled=%d maxdist=%.1f)", (int)g_enabled, g_max_distance);

  rcp->commands_hook->Add(
      "/rcpchase", {"/rcpchasecam"},
      "Third-person chase cam. '/rcpchase on|off', '/rcpchase maxdist <10-300|native>' (how far the wheel can zoom "
      "out).",
      [](std::vector<std::string> &args) {
        try {
          if (args.size() >= 2 && (args[1] == "off" || args[1] == "0")) {
            g_enabled = false;
          } else if (args.size() >= 2 && args[1] == "on") {
            g_enabled = true;
          } else if (args.size() >= 3 && (args[1] == "maxdist" || args[1] == "dist" || args[1] == "distance")) {
            g_max_distance =
                (args[2] == "native" || args[2] == "0") ? 0.0f : clampf(std::stof(args[2]), 10.0f, 300.0f);
            g_enabled = true;
          } else {
            g_enabled = !g_enabled;  // Bare /rcpchase toggles.
          }
        } catch (...) {
          Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpchase on|off | maxdist <10-300|native>'");
          return true;
        }
        apply_max_zoom();
        save_settings();
        print_status();
        return true;
      });
  logger::log("[chase] /rcpchase registered");
}
