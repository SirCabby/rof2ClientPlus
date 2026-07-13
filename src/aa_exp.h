// rof2ClientPlus - automatic AA-experience gating.
//
// Auto-manages the character's AA experience percentage based on how far into
// the current level you are. You pick a THRESHOLD (0-100% of the current
// level's XP bar) and an ACTIVE percentage (0-100%, the AA % to use when on).
// Each tick, while in game:
//   - level XP% <  threshold  -> AA experience set to 0%   (all XP levels you)
//   - level XP% >= threshold  -> AA experience set to the ACTIVE %
//
// The value is written to the client's PercentEXPtoAA field and pushed to the
// server exactly the way the AA window's +/- buttons do (write the byte, then
// call the client's send-AA-% routine) - so the server actually redistributes
// gained XP; a bare field write would do nothing, since the server owns the
// split. Off by default; persists to rof2ClientPlus.ini [Experience]. Driven by
// /rcpaaexp and the General tab of /rcpoptions.
#pragma once

class RcpService;

namespace aa_exp {
// Called every frame from ProcessGameEvents_hk (throttled + rcp_guard-wrapped,
// in-game only): enforces the AA-% gating when enabled. Cheap no-op when off.
void on_frame();

// Force the next on_frame() to re-evaluate immediately (skips the throttle) so a
// settings change from the command or the options window applies right away.
void apply_now();
}  // namespace aa_exp

// Live accessors for the /rcpaaexp command and the options-window General tab.
namespace aa_exp_settings {
bool get_enabled();
void set_enabled(bool on);      // Applies on the next tick + persists.
int get_threshold();            // 0..100 (percent into the current level to switch AA on).
void set_threshold(int pct);    // Clamped 0..100; persists.
int get_active_pct();           // 0..100 (AA % applied when active; snapped to native 10% steps).
void set_active_pct(int pct);   // Snapped to nearest 10; persists.

// Read-outs for status display; -1 if unavailable (not in game / no character yet).
int current_level_pct();        // 0..100 progress into the current level.
int current_aa_pct();           // 0..100 the client's live PercentEXPtoAA.
}  // namespace aa_exp_settings

// Registers /rcpaaexp and loads settings. Constructed by RcpService; the per-frame
// enforcement lives in the aa_exp namespace (driven from dllmain, like window_watch).
class AaExp {
 public:
  explicit AaExp(RcpService *rcp);

 private:
  RcpService *rcp_;
};
