// rof2ClientPlus - options-window UI (stock RoF2 SIDL/EQUI port).
//
// A native SIDL options window, TABBED: General / Mouse / Nameplate / Colors /
// Display (which now includes the chase-camera controls) / Ring / Sounds / Combat.
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

#include <string>
#include <vector>

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
  void populate_graphic_combo();     // Fill the ring-graphic combobox from disk + select the current one.
  void populate_sound_add_combo();   // Fill the "add sound" combobox from recently-played untracked sounds.
  void refresh_sound_list();         // Repaint the tracked-sound rows + selection highlight + volume slider.

  static constexpr int kTabCount = 8;   // General / Mouse / Nameplate / Colors / Display / Ring / Sounds / Combat.
  static constexpr int kNpCount = 7;    // Nameplate toggle checkboxes (kNpChildNames).
  static constexpr int kRoleCount = 18; // Color roles; == nameplate_colors::count().

  void *wnd_ = nullptr;
  void *btn_tab_[kTabCount] = {};
  // Mouse tab.
  void *cb_enabled_ = nullptr;
  void *cb_lockmouse_ = nullptr;
  void *cb_equip_ = nullptr;  // "Right-click to equip" (equip_item_settings).
  void *sl_sensx_ = nullptr;
  void *sl_sensy_ = nullptr;
  void *sl_smooth_ = nullptr;
  void *lbl_sensx_hdr_ = nullptr;   // static "Sensitivity X" label (bound for tab show/hide)
  void *lbl_sensy_hdr_ = nullptr;
  void *lbl_smooth_hdr_ = nullptr;
  void *lbl_sensx_ = nullptr;       // numeric value labels
  void *lbl_sensy_ = nullptr;
  void *lbl_smooth_ = nullptr;
  // Chase-camera controls (shown at the bottom of the Display tab; formerly their own Camera tab).
  void *lbl_cam_hdr_ = nullptr;  // "Chase camera" section header.
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
  void *sl_far_ = nullptr;  // Terrain view distance (far clip), world units (view_distance_settings).
  void *lbl_far_hdr_ = nullptr;
  void *lbl_far_ = nullptr;
  void *sl_actor_ = nullptr;  // Actor (NPC/player) view distance, world units.
  void *lbl_actor_hdr_ = nullptr;
  void *lbl_actor_ = nullptr;
  // Ring tab (solid-color target ring; target_ring_settings).
  void *cb_ring_enabled_ = nullptr;
  void *cb_ring_hideself_ = nullptr;
  void *cb_ring_concolor_ = nullptr;  // Color by target con level instead of the fixed color.
  void *btn_ring_color_ = nullptr;  // Color swatch: tinted live, opens the picker on click.
  void *sl_ring_outer_ = nullptr;
  void *lbl_ring_outer_hdr_ = nullptr;
  void *lbl_ring_outer_ = nullptr;
  void *sl_ring_inner_ = nullptr;
  void *lbl_ring_inner_hdr_ = nullptr;
  void *lbl_ring_inner_ = nullptr;
  void *sl_ring_opacity_ = nullptr;
  void *lbl_ring_opacity_hdr_ = nullptr;
  void *lbl_ring_opacity_ = nullptr;
  void *lbl_ring_graphic_hdr_ = nullptr;    // "Ring graphic:" static header.
  void *combo_ring_graphic_ = nullptr;      // Native Combobox: choices == available graphics.
  void *cb_ring_spin_ = nullptr;            // "Rotate ring graphic" (spin vs face-heading).
  void *cb_ring_melee_ = nullptr;           // "Scale to melee range" (outer edge tracks target melee range).
  // Sounds tab (track + adjust individual sounds; sound_settings).
  void *lbl_snd_add_ = nullptr;
  void *combo_snd_add_ = nullptr;   // "Add a played sound to the list" picker (recent untracked sounds).
  void *lbl_snd_list_ = nullptr;
  void *list_snd_ = nullptr;        // Scrollable tracked-sound list (native CListWnd): rows show name + volume.
  void *lbl_snd_vol_hdr_ = nullptr;
  void *sl_snd_vol_ = nullptr;      // Volume of the selected tracked sound (0..100; 0 = mute).
  void *lbl_snd_vol_ = nullptr;
  void *btn_snd_reset_ = nullptr;   // "Remove selected from list" (untrack the selected sound).
  // General tab (window title + Zeal-style chat timestamps).
  void *cb_windowtitle_ = nullptr;  // "Show character name in window title" (window_watch::set_char_title).
  void *cb_timestamp_ = nullptr;    // "Show chat timestamps" (chat_timestamp_settings).
  void *lbl_timestamp_hint_ = nullptr;  // Static hint pointing at /timestamp format.
  // Automatic AA experience (aa_exp_settings): enable + threshold slider + active-% slider + live status.
  void *lbl_aa_hdr_ = nullptr;          // "Automatic AA experience" section header.
  void *cb_aa_enabled_ = nullptr;
  void *lbl_aa_thresh_hdr_ = nullptr;
  void *sl_aa_thresh_ = nullptr;        // 0..100: percent into the current level to switch AA on.
  void *lbl_aa_thresh_ = nullptr;
  void *lbl_aa_active_hdr_ = nullptr;
  void *sl_aa_active_ = nullptr;        // 0..10 -> 0..100 AA% in native 10% steps.
  void *lbl_aa_active_ = nullptr;
  void *lbl_aa_status_ = nullptr;       // Live "level XP nn% / AA nn%" readout.
  // Combat tab (floating combat damage; floating_damage_settings).
  void *cb_fcd_enabled_ = nullptr;
  void *cb_fcd_mine_ = nullptr;
  void *cb_fcd_incoming_ = nullptr;
  void *cb_fcd_others_ = nullptr;
  void *cb_fcd_melee_ = nullptr;
  void *cb_fcd_spells_ = nullptr;
  void *sl_fcd_big_ = nullptr;
  void *lbl_fcd_big_hdr_ = nullptr;
  void *lbl_fcd_big_ = nullptr;
  void *btn_fcd_col_mine_ = nullptr;      // Color swatches: tinted live, open the picker on click.
  void *btn_fcd_col_incoming_ = nullptr;
  void *btn_fcd_col_other_ = nullptr;
  void *btn_fcd_col_crit_ = nullptr;
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
  bool last_equip_ = false;
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
  int last_far_ = -1;
  int last_actor_ = -1;
  bool last_ring_enabled_ = false;
  bool last_ring_hideself_ = false;
  bool last_ring_concolor_ = false;
  bool last_ring_color_ = false;
  int last_ring_outer_ = -1;
  int last_ring_inner_ = -1;
  int last_ring_opacity_ = -1;
  bool last_ring_spin_ = false;
  bool last_ring_melee_ = false;
  // Sounds tab state.
  std::string snd_selected_;                  // stem of the selected tracked sound ("" = none).
  bool snd_selection_dirty_ = false;          // set on a selection change -> refresh re-syncs the slider.
  std::vector<std::string> snd_row_stems_;    // list row index -> stem (mirrors the CListWnd row order).
  std::vector<std::string> snd_row_texts_;    // list row index -> current row text (so we update in place, not rebuild).
  std::vector<std::string> snd_add_choices_;  // add-combo index -> display name (index 0 is the placeholder).
  int last_snd_add_choice_ = -1;
  int last_snd_sel_row_ = -1;                 // last CListWnd GetCurSel we saw (detect user row clicks).
  int last_snd_vol_ = -1;
  bool last_snd_reset_ = false;
  // General tab state.
  bool last_windowtitle_ = false;
  bool last_timestamp_ = false;
  bool last_aa_enabled_ = false;
  int last_aa_thresh_ = -1;
  int last_aa_active_ = -1;
  // Combat tab state.
  bool last_fcd_enabled_ = false;
  bool last_fcd_mine_ = false;
  bool last_fcd_incoming_ = false;
  bool last_fcd_others_ = false;
  bool last_fcd_melee_ = false;
  bool last_fcd_spells_ = false;
  int last_fcd_big_ = -1;
  bool last_fcd_col_mine_ = false;      // momentary swatch-click latches
  bool last_fcd_col_incoming_ = false;
  bool last_fcd_col_other_ = false;
  bool last_fcd_col_crit_ = false;
  // Ring-graphic combobox: the choice list (index -> name, "None" first) currently loaded into the
  // combo, and the last selected index we applied (so the poll only reacts to real user changes).
  std::vector<std::string> graphic_choices_;
  int last_ring_graphic_choice_ = -1;
};
