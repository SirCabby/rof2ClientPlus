// rof2ClientPlus - options-window UI (stock RoF2 SIDL/EQUI port).
//
// A native SIDL options window, TABBED: Mouse / Camera / Nameplate / Colors / Display.
// The tab strip is a row of checkbox buttons + per-tab control groups whose
// visibility this class toggles (a real SIDL TabBox renders but does not route
// mouse input when the window is instantiated at runtime, so only proven
// primitives are used). The Colors tab is Zeal-style: one button per nameplate
// color role, its text tinted the live color (CXWnd::CRNormal), and clicking
// it opens the client's stock color picker (CColorPickerWnd::Open) whose RGB
// slider values are polled and applied live while it is open for us.
//
// The window's SIDL defs ride INSIDE the shipped uifiles/rcp/
// EQUI_OptionsWindow.xml override (the custom-UI-skin channel), so the
// client's own UI load registers the template with zero involvement from this
// code; /rcpoptions lazily instantiates it via CreateXWndFromTemplate.
// (Delivering the window as a separate included file crashed the client's
// world-entry UI parse; runtime LoadSidl is destructive - see
// tools/gen_rcp_options_ui.py for the full post-mortem.) Controls are polled
// each frame while the window exists. Opened/closed with /rcpoptions (and its
// own close box).
#pragma once

class RcpService;

class RcpOptionsUI {
 public:
  RcpOptionsUI(RcpService *rcp);

  // Called every frame from the main loop; syncs control values <-> settings
  // while the window is visible (cheap early-out otherwise).
  void on_frame();

 private:
  void toggle_window();          // /rcpoptions
  void create_window();          // Lazy: build the window + bind its controls.
  void update_labels();          // Refresh the numeric value labels from the settings.
  void sync_controls();          // Push all live settings into the controls (on open).
  void seed_last_values();       // Snapshot control values so the poll ignores the sync.
  void set_active_tab(int tab);  // Latch the tab strip + show/hide the tab groups.
  void refresh_role_tints();     // Paint each color-role button with its current color.
  void open_color_picker(int role);  // Open the stock picker seeded with a role's color.

  static constexpr int kTabCount = 5;   // Mouse / Camera / Nameplate / Colors / Display.
  static constexpr int kNpCount = 7;    // Nameplate toggle checkboxes (kNpChildNames).
  static constexpr int kRoleCount = 18; // Color roles; == nameplate_colors::count().

  void *wnd_ = nullptr;
  void *btn_tab_[kTabCount] = {};
  // Mouse tab.
  void *cb_enabled_ = nullptr;
  void *cb_lockmouse_ = nullptr;
  void *sl_sensx_ = nullptr;
  void *sl_sensy_ = nullptr;
  void *sl_smooth_ = nullptr;
  void *lbl_sensx_hdr_ = nullptr;   // static "Sensitivity X" label (bound for tab show/hide)
  void *lbl_sensy_hdr_ = nullptr;
  void *lbl_smooth_hdr_ = nullptr;
  void *lbl_sensx_ = nullptr;       // numeric value labels
  void *lbl_sensy_ = nullptr;
  void *lbl_smooth_ = nullptr;
  // Camera tab.
  void *cb_chase_enabled_ = nullptr;
  void *cb_chase_collision_ = nullptr;
  void *sl_chase_dist_ = nullptr;
  void *lbl_chase_dist_hdr_ = nullptr;
  void *lbl_chase_dist_ = nullptr;
  // Nameplate tab (checkboxes, indexed by kNpChildNames order; plus blink-speed slider).
  void *cb_np_billboard_ = nullptr;  // Custom-font billboard nameplates (drives font_overlay).
  void *cb_np_hp_ = nullptr;         // Per-bar toggles (font_overlay).
  void *cb_np_mana_ = nullptr;
  void *cb_np_stam_ = nullptr;
  void *cb_np_[kNpCount] = {};
  void *sl_blink_ = nullptr;
  void *lbl_blink_hdr_ = nullptr;
  void *lbl_blink_ = nullptr;
  void *sl_np_dist_ = nullptr;  // Max nameplate draw distance (font_overlay).
  void *lbl_np_dist_hdr_ = nullptr;
  void *lbl_np_dist_ = nullptr;
  // Colors tab (role buttons, indexed by NpColorRole order).
  void *btn_role_[kRoleCount] = {};
  // Display tab.
  void *cb_nofog_ = nullptr;
  bool create_attempted_ = false;

  // Tab state.
  int active_tab_ = 0;
  bool last_tab_[kTabCount] = {};

  // Stock color-picker session: which role it is editing (-1 = none) and the
  // last color we read from its RGB sliders (so we only re-apply on change).
  int picker_role_ = -1;
  int last_picker_rgb_ = -1;

  // Last control values seen, so the frame poll applies only real user changes
  // (a raw slider/checkbox value differing from last frame) rather than
  // continuously overwriting settings (which would fight the /rcp* commands).
  bool last_enabled_ = false;
  bool last_lockmouse_ = false;
  int last_vx_ = -1, last_vy_ = -1, last_vs_ = -1;
  bool last_chase_enabled_ = false;
  bool last_chase_collision_ = false;
  int last_chase_dist_ = -1;
  bool last_np_billboard_ = false;
  bool last_np_hp_ = false, last_np_mana_ = false, last_np_stam_ = false;
  bool last_np_[kNpCount] = {};
  int last_blink_ = -1;
  int last_np_dist_ = -1;
  bool last_role_[kRoleCount] = {};
  bool last_nofog_ = false;
};
