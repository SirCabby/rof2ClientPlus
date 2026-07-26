// rof2ClientPlus - respawn-window auto-close.
//
// The stock RoF2 "RespawnWnd" (the death chooser: Resurrect / Return to Bind) is
// dismissed by the client only when it next sees the player alive, and on this
// server that dismissal doesn't fire on the return-to-bind path, so the window
// lingers after you respawn. This closes it purely client-side, touching nothing
// else: each frame, if RespawnWnd is visible AND the local player's RespawnTimer
// is 0 (eqlib: "0 when you're alive" - i.e. you've already picked an option and
// respawned), hide it. While hovering-dead and deciding, RespawnTimer is non-zero,
// so the chooser is left alone. No server involvement.
#pragma once

class RcpService;

class RespawnClose {
 public:
  explicit RespawnClose(RcpService *rcp);
  void on_frame();  // Driven each frame from ProcessGameEvents_hk (dllmain.cpp).

 private:
  bool have_anchor_ = false;               // recorded the death-spot position while the chooser is up
  float ax_ = 0.f, ay_ = 0.f, az_ = 0.f;   // that anchor (player pos when the chooser first appeared)
  int closing_frames_ = 0;                 // after a detected respawn, keep dismissing for a short burst
};
