// rof2ClientPlus - mouse handling (the /rcpcam feature), stock RoF2 port.
//
// This starts as a read-only probe that confirms how the client drives
// mouse-look (which delta the client uses and how heading responds), so the
// actual smoothing can be wired to the right memory without crash-testing
// blindly. Addresses are stock RoF2 facts sourced from eqlib.
#pragma once

class RcpService;

class MouseMods {
 public:
  MouseMods(RcpService *rcp);

  // Installs the DInput GetDeviceState hook the first time we're in-game. Kept
  // out of login/char-select so nothing of ours touches the input path there
  // (that path shares threads with DXVK/Wine and was corrupting at char select).
  void ensure_hooked();

 private:
  RcpService *rcp_ = nullptr;
  bool hooked_ = false;
};

// Accessors for the options-window UI to read/write the live mouse settings.
namespace mouse_settings {
float get_sens_x();
float get_sens_y();
float get_smoothing();
bool get_enabled();
bool get_lock_mouse();
void set(float sens_x, float sens_y, float smoothing, bool enabled);  // Applies live + persists to ini.
void set_lock_mouse(bool lock);  // Toggles cursor-confine-to-window + persists to ini.

// Re-clips the OS cursor to the game window each frame while lock-to-window is on
// and the game is focused (no-op otherwise). Called from the main loop.
void apply_cursor_lock();
}  // namespace mouse_settings
