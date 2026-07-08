// rof2ClientPlus - options-window UI (stock RoF2 SIDL/EQUI port).
//
// A native SIDL options window (uifiles/rcp/EQUI_RcpOptions.xml) with a Mouse
// tab wired to the mouse-look sensitivity/smoothing settings. Built against
// eqlib's stock-RoF2 layouts. The window definition is registered by calling
// CSidlManagerBase::LoadSidl on our own file after the client's EQUI.xml loads;
// the window is created lazily on /rcpoptions and its controls are polled each
// frame while visible. Opened/closed with /rcpoptions (and its own close box).
#pragma once

class RcpService;

class RcpOptionsUI {
 public:
  RcpOptionsUI(RcpService *rcp);

  // Called every frame from the main loop; syncs control values <-> settings
  // while the window is visible (cheap early-out otherwise).
  void on_frame();

 private:
  void toggle_window();   // /rcpoptions
  void create_window();   // Lazy: build the window + bind its controls.
  void update_labels();   // Refresh the numeric value labels from the settings.

  void *wnd_ = nullptr;
  void *cb_enabled_ = nullptr;
  void *cb_lockmouse_ = nullptr;
  void *sl_sensx_ = nullptr;
  void *sl_sensy_ = nullptr;
  void *sl_smooth_ = nullptr;
  void *lbl_sensx_ = nullptr;
  void *lbl_sensy_ = nullptr;
  void *lbl_smooth_ = nullptr;
  bool create_attempted_ = false;

  // Last control values seen, so the frame poll applies only real user changes
  // (a raw slider/checkbox value differing from last frame) rather than
  // continuously overwriting settings (which would fight /rcpcam).
  bool last_enabled_ = false;
  bool last_lockmouse_ = false;
  int last_vx_ = -1, last_vy_ = -1, last_vs_ = -1;
};
