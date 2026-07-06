// rof2ClientPlus - mouse-look sensitivity + smoothing (the /rcpcam feature).
//
// Stock RoF2 has no separable procMouse to replace (Zeal's approach): the
// mouse-look delta (DInput lX/lY at 0xE67884) is fetched by GetDeviceState and
// consumed in the same function (0x5F9E30, the only reader). So we intercept one
// level lower - the mouse device's IDirectInputDevice8::GetDeviceState - and
// rewrite lX/lY in the buffer right after the fetch, but ONLY while mouse-look is
// engaged (EverQuestInfo->MouseLook, 0xDDF702), so the free cursor stays crisp.
//
// The stock client turns from the INTEGER mouse delta (fildl at 0x516d40), so a
// slow axis's sparse whole counts (0,0,0,1) become visible stair-steps on
// diagonals. Rather than rewrite the integer buffer (which can't carry sub-count
// precision), we reproduce the client's turn math in FLOAT on a low-pass-smoothed
// delta and apply it directly to the controlled entity's Heading/Pitch, then
// zero the buffer so the client's own integer turn is suppressed. This is the
// first-person slice of the ZealCam camera port; F9 chase views are left to the
// client until the rest of camera_mods is ported.
//
// Off until configured; '/rcpcam <x> <y>' sets independent axis sensitivity.
// Addresses are stock RoF2 facts from eqlib + the 0x516d40 turn function.
#include "mouse_mods.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "commands.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

// Stock RoF2 addresses (eqlib offsets/eqgame.h).
static void **const kDIMouseDevice = reinterpret_cast<void **>(0xE67B50);   // DI8__Mouse (IDirectInputDevice8*)
static constexpr uintptr_t kMouseStateBuf = 0xE67884;                       // DIMOUSESTATE2 buffer (lX+0, lY+4)
static uint8_t *const kMouseLook = reinterpret_cast<uint8_t *>(0xDDF702);   // EverQuestInfo->MouseLook (1 while looking)
static uint8_t *const kRBtn = reinterpret_cast<uint8_t *>(0xDDF73D);        // EverQuestInfo->MouseButtons[1] (right down)
static constexpr int kGetDeviceStateVtIndex = 9;                            // IDirectInputDevice8::GetDeviceState

// Float-turn addresses (from the stock RoF2 mouse-turn function at 0x516d40).
// The client turns by (delta / screen_dim) * base * S applied to an integer
// delta, giving sparse-count stair-stepping on the slow axis. We reproduce the
// SAME math in float on a smoothed delta so diagonals stay smooth.
static void **const kControlled = reinterpret_cast<void **>(0xDD2644);      // controlled player (self/mount/charm)
static int *const kCameraType = reinterpret_cast<int *>(0xD1FD9C);          // CDisplay::cameraType (0 = first person)
static int *const kScreenLeft = reinterpret_cast<int *>(0xDDF620);
static int *const kScreenRight = reinterpret_cast<int *>(0xDDF628);
static int *const kScreenTop = reinterpret_cast<int *>(0xDDF624);
static int *const kScreenBottom = reinterpret_cast<int *>(0xDDF62C);
static int *const kMouseSensIni = reinterpret_cast<int *>(0xDDF69C);        // client MouseSensitivity setting
static constexpr int kOffHeading = 0x80;       // PlayerClient::Heading (float, 0..512 yaw)
static constexpr int kOffSpeedHeading = 0x8C;  // PlayerClient::SpeedHeading (yaw rate)
static constexpr int kOffPitch = 0x90;         // PlayerClient::CameraAngle (look pitch)

typedef long(__stdcall *GetDeviceState_t)(void *self, unsigned long cb, void *data);

static GetDeviceState_t g_orig_gds = nullptr;
static bool g_enabled = false;               // Off = stock client behavior.
static float g_sens_x = 1.0f, g_sens_y = 1.0f;   // Independent mouse-look axis multipliers.
static float g_strength = 0.0f;              // Optional low-pass weight toward the previous poll (0 = off).
static float g_carry_x = 0.0f, g_carry_y = 0.0f;  // Fractional remainders (exact scaling of small deltas).
static float g_smooth_x = 0.0f, g_smooth_y = 0.0f;
static int g_log_count = 0;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Persistence to rof2ClientPlus.ini (next to eqgame.exe). We use IO_ini directly
// rather than RcpSetting because RcpSetting's ctor registers a CleanCharSelectUI
// callback through the (not-yet-ported) CallbackManager.
static constexpr char kIniSection[] = "Mouse";

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "SensitivityX")) g_sens_x = ini.getValue<float>(kIniSection, "SensitivityX");
  if (ini.exists(kIniSection, "SensitivityY")) g_sens_y = ini.getValue<float>(kIniSection, "SensitivityY");
  if (ini.exists(kIniSection, "Smoothing")) g_strength = ini.getValue<float>(kIniSection, "Smoothing");
  if (ini.exists(kIniSection, "Enabled")) g_enabled = ini.getValue<bool>(kIniSection, "Enabled");
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<float>(kIniSection, "SensitivityX", g_sens_x);
  ini.setValue<float>(kIniSection, "SensitivityY", g_sens_y);
  ini.setValue<float>(kIniSection, "Smoothing", g_strength);
  ini.setValue<bool>(kIniSection, "Enabled", g_enabled);
}

namespace mouse_settings {
float get_sens_x() { return g_sens_x; }
float get_sens_y() { return g_sens_y; }
float get_smoothing() { return g_strength; }
bool get_enabled() { return g_enabled; }
void set(float sens_x, float sens_y, float smoothing, bool enabled) {
  g_sens_x = clampf(sens_x, 0.05f, 20.0f);
  g_sens_y = clampf(sens_y, 0.05f, 20.0f);
  g_strength = clampf(smoothing, 0.0f, 0.9f);
  g_enabled = enabled;
  if (!g_enabled) g_carry_x = g_carry_y = g_smooth_x = g_smooth_y = 0.0f;
  save_settings();
}
}  // namespace mouse_settings

static long __stdcall GetDeviceState_hk(void *self, unsigned long cb, void *data) {
  // g_orig_gds is captured at install time (main thread) so this hook never
  // touches the hook map - DInput can poll from a background thread, and racing
  // the map on the first call intermittently corrupted state at char select.
  long r = g_orig_gds(self, cb, data);

  // Only the mouse immediate-state fetch (this exact buffer), and only while the
  // RIGHT button is held for mouse-look (player turn). Left-button look pans the
  // camera and must stay untouched, so we require the right-button state too.
  if (g_enabled && r == 0 && reinterpret_cast<uintptr_t>(data) == kMouseStateBuf && *kMouseLook && *kRBtn) {
    int32_t *lX = reinterpret_cast<int32_t *>(kMouseStateBuf);
    int32_t *lY = reinterpret_cast<int32_t *>(kMouseStateBuf + 4);
    const int32_t raw_x = *lX, raw_y = *lY;

    // Only take over the turn in first person; the client's other camera modes
    // (F9 chase views) stay untouched until the full camera port handles them.
    void *controlled = *kControlled;
    if (controlled && *kCameraType == 0) {
      // Smooth the raw delta in FLOAT. This is the crux of the fix: the client
      // turns from an INTEGER delta, so a slow axis's sparse whole counts
      // (0,0,0,1) become visible pitch jumps. Low-passing in float spreads each
      // count across the polls until the next, so diagonals move on both axes at
      // once. g_strength is the weight toward the previous poll (a light default
      // keeps it smooth without perceptible lag).
      const float t = g_strength > 0.0f ? g_strength : 0.35f;
      g_smooth_x = raw_x * (1.0f - t) + g_smooth_x * t;
      g_smooth_y = raw_y * (1.0f - t) + g_smooth_y * t;

      // Reproduce the stock turn calibration (0x516d40) in float, so g_sens=1
      // matches stock feel: yaw = (dx/width)*512*S, pitch = (dy/height)*256*S.
      float width = static_cast<float>(*kScreenRight - *kScreenLeft);
      float height = static_cast<float>(*kScreenBottom - *kScreenTop);
      if (width < 1.0f) width = 1920.0f;
      if (height < 1.0f) height = 1080.0f;
      const float S = (static_cast<float>(*kMouseSensIni) - 1.0f) * 0.1428571f * 1.5f + 0.5f;

      float *heading = reinterpret_cast<float *>(static_cast<char *>(controlled) + kOffHeading);
      float *pitch = reinterpret_cast<float *>(static_cast<char *>(controlled) + kOffPitch);
      const float d_head = g_sens_x * (g_smooth_x / width) * 512.0f * S;
      const float d_pitch = g_sens_y * (g_smooth_y / height) * 256.0f * S;

      *heading = fmodf(*heading - d_head + 512.0f, 512.0f);  // Wrap 0..512.
      *pitch = clampf(*pitch - d_pitch, -128.0f, 128.0f);    // Mouse up looks up.
      *reinterpret_cast<float *>(static_cast<char *>(controlled) + kOffSpeedHeading) = 0.0f;  // Cancel client yaw rate.

      if (g_log_count < 40 && (raw_x || raw_y)) {
        logger::logf("[mouse] raw=(%d,%d) sm=(%.2f,%.2f) dH=%.3f dP=%.3f S=%.2f", raw_x, raw_y, g_smooth_x, g_smooth_y,
                     d_head, d_pitch, S);
        ++g_log_count;
      }

      // Suppress the client's own integer turn (it reads this buffer next) ONLY
      // when we replaced it. In other camera modes we leave the buffer intact so
      // the client's native turn keeps working until the chase-camera phase.
      *lX = 0;
      *lY = 0;
    }
  }
  return r;
}

static void print_status() {
  char msg[160];
  std::snprintf(msg, sizeof(msg), "rof2ClientPlus mouse-look: %s | sensitivity X=%.2f Y=%.2f | smoothing=%.2f",
                g_enabled ? "ON" : "OFF", g_sens_x, g_sens_y, g_strength);
  Rcp::Game::print_chat(msg);
}

void MouseMods::ensure_hooked() {
  if (hooked_ || !Rcp::Game::is_in_game()) return;
  hooked_ = true;  // Set first so a null device doesn't retry every frame.

  void *device = *kDIMouseDevice;
  if (!device) {
    logger::log("[mouse] DI mouse device is null; hook NOT installed");
    return;
  }
  // The device's first member is its vtable; GetDeviceState is slot 9 (+0x24).
  // Capture the original BEFORE patching so the hook can call it without ever
  // reading the hook map (which is not safe against the DInput poll thread).
  int *vtable = *reinterpret_cast<int **>(device);
  g_orig_gds = reinterpret_cast<GetDeviceState_t>(vtable[kGetDeviceStateVtIndex]);
  int slot_addr = reinterpret_cast<int>(&vtable[kGetDeviceStateVtIndex]);
  rcp_->hooks->Add("mouse_gds", slot_addr, GetDeviceState_hk, hook_type_vtable);
  logger::logf("[mouse] GetDeviceState hook installed in-game (device=%p vtable-slot=0x%x)", device, slot_addr);
}

MouseMods::MouseMods(RcpService *rcp) : rcp_(rcp) {
  load_settings();  // Restore persisted sensitivity/smoothing from rof2ClientPlus.ini.
  logger::logf("[mouse] settings loaded: enabled=%d sensX=%.2f sensY=%.2f smooth=%.2f (hook deferred until in-game)",
               (int)g_enabled, g_sens_x, g_sens_y, g_strength);

  rcp->commands_hook->Add(
      "/rcpcam", {"/rcpsmoothing"},
      "Mouse-look sensitivity: '/rcpcam <x> <y>' (e.g. /rcpcam 2 1.4), '/rcpcam <x> <y> <smooth 0-0.9>', '/rcpcam off'.",
      [](std::vector<std::string> &args) {
        try {
          if (args.size() >= 2 && (args[1] == "off" || args[1] == "0")) {
            g_enabled = false;
          } else if (args.size() >= 4) {
            g_sens_x = clampf(std::stof(args[1]), 0.05f, 20.0f);
            g_sens_y = clampf(std::stof(args[2]), 0.05f, 20.0f);
            g_strength = clampf(std::stof(args[3]), 0.0f, 0.9f);
            g_enabled = true;
            g_log_count = 0;
          } else if (args.size() == 3) {
            g_sens_x = clampf(std::stof(args[1]), 0.05f, 20.0f);
            g_sens_y = clampf(std::stof(args[2]), 0.05f, 20.0f);
            g_enabled = true;
            g_log_count = 0;
          } else if (args.size() == 2) {
            g_sens_x = g_sens_y = clampf(std::stof(args[1]), 0.05f, 20.0f);
            g_enabled = true;
            g_log_count = 0;
          } else {
            g_enabled = !g_enabled;
            if (g_enabled) g_log_count = 0;
          }
        } catch (...) {
          Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpcam <x> <y>' e.g. '/rcpcam 2 1.4'");
          return true;
        }
        if (!g_enabled) g_carry_x = g_carry_y = g_smooth_x = g_smooth_y = 0.0f;
        save_settings();
        print_status();
        return true;
      });
  logger::log("[mouse] /rcpcam registered");
}
