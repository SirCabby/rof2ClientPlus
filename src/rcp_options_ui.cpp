// rof2ClientPlus - options-window UI (stock RoF2 SIDL/EQUI port), tabbed build.
#include "rcp_options_ui.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "chase_cam.h"
#include "commands.h"
#include "game_functions.h"
#include "logger.h"
#include "mouse_mods.h"
#include "nameplate.h"
#include "rcp.h"

// ---- stock RoF2 addresses (eqlib offsets + disasm of the client's own usage) ----
static constexpr int kCreateXWnd = 0x870400;              // CSidlManagerBase::CreateXWndFromTemplate(parent,name)
static constexpr int kGetChildItem = 0x85CFD0;            // CSidlScreenWnd::GetChildItem(name, flag)
static constexpr int kSliderGetValue = 0x895FE0;          // CSliderWnd::GetValue()
static constexpr int kSliderSetValue = 0x8961B0;          // CSliderWnd::SetValue(int)
static constexpr int kCXStrCtor = 0x805C20;               // CXStr::CXStr(const char*)
static void **const kSidlManager = reinterpret_cast<void **>(0x15D3D08);  // pinstCSidlManager
static constexpr int kShowVtOffset = 0xD8;                // CXWnd vtable Show() slot
static constexpr int kCheckedOffset = 0x1E4;              // CButtonWnd::Checked
static constexpr int kDShowOffset = 0x196;                // CXWnd::dShow (visible flag)
static constexpr int kCRNormalOffset = 0x12C;             // CXWnd::CRNormal (ARGB text color)

// Stock color picker (eqlib CColorPickerWnd + disasm of the client's own Open
// call sites, e.g. 0x63fc3f: mov ecx,[0xD1FC64]; push color; push caller;
// call 0x659AF0). Open = __thiscall(this, CXWnd* caller, D3DCOLOR), ret 0x8.
static void **const kColorPicker = reinterpret_cast<void **>(0xD1FC64);  // pinstCColorPickerWnd
static constexpr int kColorPickerOpen = 0x659AF0;
static constexpr int kPickerCallerOffset = 0x22C;  // CColorPickerWnd::pwndCaller
static constexpr int kPickerRedOffset = 0x270;     // CColorPickerWnd::RedSliderValue
static constexpr int kPickerGreenOffset = 0x27C;   // CColorPickerWnd::GreenSliderValue
static constexpr int kPickerBlueOffset = 0x288;    // CColorPickerWnd::BlueSliderValue

// ---- thin wrappers over the client's __thiscall methods ----

// Constructs a CXStr (4-byte value) in-place from a C string. Leaks the CStrRep
// (a few bytes, a handful of times) rather than risk a wrong destructor.
static void cxstr_init(void *buf, const char *s) {
  *reinterpret_cast<uint32_t *>(buf) = 0;
  reinterpret_cast<void(__thiscall *)(void *, const char *)>(kCXStrCtor)(buf, s);
}

static void *get_child(void *wnd, const char *name) {
  if (!wnd) return nullptr;
  uint32_t name_cxstr;
  cxstr_init(&name_cxstr, name);
  return reinterpret_cast<void *(__thiscall *)(void *, void *, int)>(kGetChildItem)(wnd, &name_cxstr, 0);
}

static int slider_get(void *slider) {
  return slider ? reinterpret_cast<int(__thiscall *)(void *)>(kSliderGetValue)(slider) : 0;
}
static void slider_set(void *slider, int v) {
  if (slider) reinterpret_cast<void(__thiscall *)(void *, int)>(kSliderSetValue)(slider, v);
}
// CSliderWnd value is at +0x218, its (exclusive) max at +0x220. Newly created
// sliders have no usable range, so we set one explicitly (0..max_inclusive).
static constexpr int kSliderMaxOffset = 0x220;
static void slider_set_range(void *slider, int max_inclusive) {
  if (slider) *reinterpret_cast<int *>(reinterpret_cast<char *>(slider) + kSliderMaxOffset) = max_inclusive + 1;
}

// CXWnd::SetWindowText is virtual at vtable offset 0x124 - the cache-correct way
// to set a label's text (it marks the label dirty so it re-draws).
static constexpr int kSetWindowTextVtOffset = 0x124;
static constexpr int kCXStrDtor = 0x465AE0;  // CXStr::~CXStr (thiscall, no args), pairs with kCXStrCtor.
static void set_label_text(void *label, const char *text) {
  if (!label) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  void *vtable = *reinterpret_cast<void **>(label);
  void *fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + kSetWindowTextVtOffset);
  reinterpret_cast<void(__thiscall *)(void *, const void *)>(fn)(label, &cxstr);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);  // Release our temp's reference.
}
static bool checkbox_get(void *cb) {
  return cb ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(cb) + kCheckedOffset) != 0 : false;
}
static void checkbox_set(void *cb, bool checked) {
  if (cb) *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(cb) + kCheckedOffset) = checked ? 1 : 0;
}
static bool is_visible(void *wnd) {
  return wnd ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(wnd) + kDShowOffset) != 0 : false;
}
static void show_window(void *wnd, bool show) {
  if (!wnd) return;
  void *vtable = *reinterpret_cast<void **>(wnd);
  void *show_fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + kShowVtOffset);
  reinterpret_cast<int(__thiscall *)(void *, int, int, int)>(show_fn)(wnd, show ? 1 : 0, 1, 1);
}
// Sets a control's normal text color (CXWnd::CRNormal, ARGB). Used to paint the
// color-role buttons with their live nameplate color (Zeal-style swatch).
static void set_text_color(void *wnd, int rgb) {
  if (!wnd) return;
  *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(wnd) + kCRNormalOffset) = 0xFF000000u | (rgb & 0xFFFFFF);
}

// ---- slider <-> setting mappings ----
// Sensitivity sliders span 0..200 -> 0..20x multiplier (0.1x per step). Smoothing
// spans 0..100 -> 0..0.9.
static constexpr int kSensSliderMax = 200;
static constexpr int kSmoothSliderMax = 100;
static int sens_to_slider(float mult) {
  int v = static_cast<int>(mult * 10.0f + 0.5f);  // 1.0x -> 10, 20x -> 200
  return v < 0 ? 0 : (v > kSensSliderMax ? kSensSliderMax : v);
}
static float slider_to_sens(int v) {
  float m = v / 10.0f;
  return m < 0.05f ? 0.05f : m;
}
static int smooth_to_slider(float s) {
  int v = static_cast<int>(s / 0.9f * kSmoothSliderMax + 0.5f);
  return v < 0 ? 0 : (v > kSmoothSliderMax ? kSmoothSliderMax : v);
}
static float slider_to_smooth(int v) { return v / static_cast<float>(kSmoothSliderMax) * 0.9f; }

// Chase max-zoom slider spans 0..300 world units; 0 means "the native max (53)".
static constexpr int kChaseDistSliderMax = 300;
static int chase_dist_to_slider(float d) {
  if (d <= 0.0f) return 0;
  int v = static_cast<int>(d + 0.5f);
  return v < 10 ? 10 : (v > kChaseDistSliderMax ? kChaseDistSliderMax : v);
}
static float chase_slider_to_dist(int v) { return v <= 0 ? 0.0f : static_cast<float>(v); }

// Blink-speed slider: 0..56 -> 200..3000 ms full blink cycle (50 ms per step).
static constexpr int kBlinkSliderMax = 56;
static int blink_to_slider(int ms) {
  int v = (ms - 200) / 50;
  return v < 0 ? 0 : (v > kBlinkSliderMax ? kBlinkSliderMax : v);
}
static int slider_to_blink(int v) { return 200 + v * 50; }

// Nameplate checkbox child names, in the positional order of nameplate_settings::set
// (also the order np_read_settings fills). Must stay in sync with cb_np_[]/last_np_[].
static const char *const kNpChildNames[] = {"Rcp_NpConColors",   "Rcp_NpStateColors",  "Rcp_NpTargetColor",
                                            "Rcp_NpTargetBlink", "Rcp_NpTargetMarker", "Rcp_NpTargetHealth",
                                            "Rcp_NpHideSelf"};
static void np_read_settings(bool out[7]) {
  out[0] = nameplate_settings::get_con_colors();
  out[1] = nameplate_settings::get_state_colors();
  out[2] = nameplate_settings::get_target_color();
  out[3] = nameplate_settings::get_target_blink();
  out[4] = nameplate_settings::get_target_marker();
  out[5] = nameplate_settings::get_target_health();
  out[6] = nameplate_settings::get_hide_self();
}

// Tab strip child names (index == tab id used throughout).
static const char *const kTabChildNames[] = {"Rcp_TabMouse", "Rcp_TabCamera", "Rcp_TabNameplate", "Rcp_TabColors"};

// ---- Window-template delivery (no code in the load path at all) ----
//
// The RcpOptions Screen + control defs live INSIDE the shipped
// uifiles/rcp/EQUI_OptionsWindow.xml override (the custom-UI-skin channel), so
// the client's own per-file skin fallback parses and registers them during its
// normal UI load. Nothing here hooks or re-runs any load. Two dead ends, for
// the record: (1) delivering the window as a separate included file (nested
// include or an EQUI.xml merge) crashed the client's world-entry UI parse once
// the window grew past ~a dozen controls (illegal-instruction into .rsrc,
// c000001d, empty UIErrors); (2) calling LoadSidl(0x870C60) at runtime is
// destructive, not additive - it clears the SIDL manager and rebuilds from the
// given root, orphaning every live window's templates.

// ---- RcpOptionsUI ----

void RcpOptionsUI::create_window() {
  create_attempted_ = true;
  void *sidlmgr = *kSidlManager;
  if (!sidlmgr) {
    logger::log("[ui] CreateWindow: SIDL manager is null");
    return;
  }
  // The RcpOptions template was registered during the client's own UI load
  // (it ships inside the EQUI_OptionsWindow.xml override); just instantiate it.
  uint32_t name_cxstr;
  cxstr_init(&name_cxstr, "RcpOptions");
  wnd_ = reinterpret_cast<void *(__thiscall *)(void *, void *, void *)>(kCreateXWnd)(sidlmgr, nullptr, &name_cxstr);
  logger::logf("[ui] CreateXWndFromTemplate('RcpOptions') = %p", wnd_);
  if (!wnd_) return;

  for (int i = 0; i < kTabCount; ++i) btn_tab_[i] = get_child(wnd_, kTabChildNames[i]);
  cb_enabled_ = get_child(wnd_, "Rcp_Enabled");
  cb_lockmouse_ = get_child(wnd_, "Rcp_LockMouse");
  sl_sensx_ = get_child(wnd_, "Rcp_SensX");
  sl_sensy_ = get_child(wnd_, "Rcp_SensY");
  sl_smooth_ = get_child(wnd_, "Rcp_Smooth");
  lbl_sensx_hdr_ = get_child(wnd_, "Rcp_SensXLabel");
  lbl_sensy_hdr_ = get_child(wnd_, "Rcp_SensYLabel");
  lbl_smooth_hdr_ = get_child(wnd_, "Rcp_SmoothLabel");
  lbl_sensx_ = get_child(wnd_, "Rcp_SensXValue");
  lbl_sensy_ = get_child(wnd_, "Rcp_SensYValue");
  lbl_smooth_ = get_child(wnd_, "Rcp_SmoothValue");
  cb_chase_enabled_ = get_child(wnd_, "Rcp_ChaseEnabled");
  cb_chase_collision_ = get_child(wnd_, "Rcp_ChaseCollision");
  sl_chase_dist_ = get_child(wnd_, "Rcp_ChaseDist");
  lbl_chase_dist_hdr_ = get_child(wnd_, "Rcp_ChaseDistLabel");
  lbl_chase_dist_ = get_child(wnd_, "Rcp_ChaseDistValue");
  for (int i = 0; i < kNpCount; ++i) cb_np_[i] = get_child(wnd_, kNpChildNames[i]);
  sl_blink_ = get_child(wnd_, "Rcp_BlinkSpeed");
  lbl_blink_hdr_ = get_child(wnd_, "Rcp_BlinkSpeedLabel");
  lbl_blink_ = get_child(wnd_, "Rcp_BlinkSpeedValue");
  char rolename[16];
  for (int i = 0; i < kRoleCount; ++i) {
    std::snprintf(rolename, sizeof(rolename), "Rcp_Role%d", i);
    btn_role_[i] = get_child(wnd_, rolename);
  }
  logger::logf("[ui] controls bound: tabs=%p,%p,%p,%p mouse(en=%p sx=%p) chase(en=%p dist=%p) np0=%p role0=%p",
               btn_tab_[0], btn_tab_[1], btn_tab_[2], btn_tab_[3], cb_enabled_, sl_sensx_, cb_chase_enabled_,
               sl_chase_dist_, cb_np_[0], btn_role_[0]);

  slider_set_range(sl_sensx_, kSensSliderMax);  // 0..200 -> 0..20x sensitivity.
  slider_set_range(sl_sensy_, kSensSliderMax);
  slider_set_range(sl_smooth_, kSmoothSliderMax);         // 0..100 -> 0..0.9 smoothing.
  slider_set_range(sl_chase_dist_, kChaseDistSliderMax);  // 0..300 world units (0 = native max).
  slider_set_range(sl_blink_, kBlinkSliderMax);           // 0..56 -> 200..3000 ms blink cycle.

  refresh_role_tints();
  set_active_tab(active_tab_);  // Latch the strip + show only the active group.
  show_window(wnd_, false);     // Created hidden; /rcpoptions reveals it.
}

// Latch the tab strip (radio behavior) and show/hide the per-tab groups.
void RcpOptionsUI::set_active_tab(int tab) {
  if (tab < 0 || tab >= kTabCount) tab = 0;
  active_tab_ = tab;
  for (int i = 0; i < kTabCount; ++i) {
    checkbox_set(btn_tab_[i], i == tab);
    last_tab_[i] = (i == tab);
  }
  void *mouse[] = {cb_enabled_, lbl_sensx_hdr_,  sl_sensx_,  lbl_sensx_,  lbl_sensy_hdr_, sl_sensy_,
                   lbl_sensy_,  lbl_smooth_hdr_, sl_smooth_, lbl_smooth_, cb_lockmouse_};
  void *camera[] = {cb_chase_enabled_, cb_chase_collision_, lbl_chase_dist_hdr_, sl_chase_dist_, lbl_chase_dist_};
  for (void *w : mouse) show_window(w, tab == 0);
  for (void *w : camera) show_window(w, tab == 1);
  for (int i = 0; i < kNpCount; ++i) show_window(cb_np_[i], tab == 2);
  show_window(lbl_blink_hdr_, tab == 2);
  show_window(sl_blink_, tab == 2);
  show_window(lbl_blink_, tab == 2);
  for (int i = 0; i < kRoleCount; ++i) show_window(btn_role_[i], tab == 3);
}

// Paint each color-role button's text with its current palette color.
void RcpOptionsUI::refresh_role_tints() {
  for (int i = 0; i < kRoleCount; ++i) set_text_color(btn_role_[i], nameplate_colors::get(i));
}

// Opens the client's stock color picker seeded with the role's current color.
// Result handling is poll-based (see on_frame): while the picker is visible
// and was opened by us, its RGB slider values are applied to the role live.
void RcpOptionsUI::open_color_picker(int role) {
  void *picker = *kColorPicker;
  if (!picker) {
    logger::log("[ui] color picker instance is null");
    return;
  }
  picker_role_ = role;
  last_picker_rgb_ = nameplate_colors::get(role);
  reinterpret_cast<int(__thiscall *)(void *, void *, uint32_t)>(kColorPickerOpen)(
      picker, wnd_, 0xFF000000u | static_cast<uint32_t>(last_picker_rgb_));
  logger::logf("[ui] color picker opened for role %d (%s)", role, nameplate_colors::name(role));
}

// Push current settings into the controls (called when opening).
void RcpOptionsUI::sync_controls() {
  checkbox_set(cb_enabled_, mouse_settings::get_enabled());
  checkbox_set(cb_lockmouse_, mouse_settings::get_lock_mouse());
  slider_set(sl_sensx_, sens_to_slider(mouse_settings::get_sens_x()));
  slider_set(sl_sensy_, sens_to_slider(mouse_settings::get_sens_y()));
  slider_set(sl_smooth_, smooth_to_slider(mouse_settings::get_smoothing()));
  checkbox_set(cb_chase_enabled_, chase_settings::get_enabled());
  checkbox_set(cb_chase_collision_, chase_settings::get_collision());
  slider_set(sl_chase_dist_, chase_dist_to_slider(chase_settings::get_max_distance()));
  bool np[kNpCount];
  np_read_settings(np);
  for (int i = 0; i < kNpCount; ++i) checkbox_set(cb_np_[i], np[i]);
  slider_set(sl_blink_, blink_to_slider(nameplate_settings::get_blink_ms()));
  refresh_role_tints();
}

// Snapshot the current raw control values so the frame poll treats the sync above
// as the baseline (not a user change) and doesn't overwrite settings next frame.
void RcpOptionsUI::seed_last_values() {
  last_enabled_ = checkbox_get(cb_enabled_);
  last_lockmouse_ = checkbox_get(cb_lockmouse_);
  last_vx_ = slider_get(sl_sensx_);
  last_vy_ = slider_get(sl_sensy_);
  last_vs_ = slider_get(sl_smooth_);
  last_chase_enabled_ = checkbox_get(cb_chase_enabled_);
  last_chase_collision_ = checkbox_get(cb_chase_collision_);
  last_chase_dist_ = slider_get(sl_chase_dist_);
  for (int i = 0; i < kNpCount; ++i) last_np_[i] = checkbox_get(cb_np_[i]);
  last_blink_ = slider_get(sl_blink_);
  for (int i = 0; i < kTabCount; ++i) last_tab_[i] = checkbox_get(btn_tab_[i]);
  for (int i = 0; i < kRoleCount; ++i) last_role_[i] = checkbox_get(btn_role_[i]);
}

void RcpOptionsUI::update_labels() {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f", mouse_settings::get_sens_x());
  set_label_text(lbl_sensx_, buf);
  std::snprintf(buf, sizeof(buf), "%.1f", mouse_settings::get_sens_y());
  set_label_text(lbl_sensy_, buf);
  std::snprintf(buf, sizeof(buf), "%.2f", mouse_settings::get_smoothing());
  set_label_text(lbl_smooth_, buf);
  // Chase max zoom shows "native" at 0, else the world-unit value.
  if (chase_settings::get_max_distance() > 0.0f)
    std::snprintf(buf, sizeof(buf), "%.0f", chase_settings::get_max_distance());
  else
    std::snprintf(buf, sizeof(buf), "native");
  set_label_text(lbl_chase_dist_, buf);
  // Blink period in seconds.
  std::snprintf(buf, sizeof(buf), "%.2fs", nameplate_settings::get_blink_ms() / 1000.0f);
  set_label_text(lbl_blink_, buf);
}

void RcpOptionsUI::toggle_window() {
  if (!wnd_ && !create_attempted_) create_window();
  if (!wnd_) {
    Rcp::Game::print_chat("rof2ClientPlus options window unavailable (is uifiles/rcp installed?).");
    return;
  }
  bool make_visible = !is_visible(wnd_);
  if (make_visible) {
    sync_controls();
    update_labels();
    set_active_tab(active_tab_);  // Re-assert group visibility + tab latches.
    seed_last_values();           // Baseline so the poll doesn't read the sync as a user change.
  }
  show_window(wnd_, make_visible);
}

void RcpOptionsUI::on_frame() {
  // Only ever touch the window while in-game. When not in-game (login, char
  // select, zoning) the client tears its UI down, so drop our handles to avoid
  // ever dereferencing a destroyed window; they are rebuilt on next /rcpoptions.
  if (!Rcp::Game::is_in_game()) {
    wnd_ = cb_enabled_ = cb_lockmouse_ = sl_sensx_ = sl_sensy_ = sl_smooth_ = nullptr;
    lbl_sensx_hdr_ = lbl_sensy_hdr_ = lbl_smooth_hdr_ = lbl_sensx_ = lbl_sensy_ = lbl_smooth_ = nullptr;
    cb_chase_enabled_ = cb_chase_collision_ = sl_chase_dist_ = lbl_chase_dist_hdr_ = lbl_chase_dist_ = nullptr;
    sl_blink_ = lbl_blink_hdr_ = lbl_blink_ = nullptr;
    for (int i = 0; i < kTabCount; ++i) btn_tab_[i] = nullptr;
    for (int i = 0; i < kNpCount; ++i) cb_np_[i] = nullptr;
    for (int i = 0; i < kRoleCount; ++i) btn_role_[i] = nullptr;
    picker_role_ = -1;
    create_attempted_ = false;
    return;
  }
  // Poll whenever the window handle exists (in-game already gated above). We do
  // NOT gate on is_visible: a hidden window's controls simply don't change, so
  // polling them is harmless, and it avoids depending on the visibility flag.
  if (!wnd_) return;

  // Tab strip: any checked-state change switches to (or re-latches) a tab.
  for (int i = 0; i < kTabCount; ++i) {
    bool c = checkbox_get(btn_tab_[i]);
    if (c == last_tab_[i]) continue;
    if (c && i != active_tab_) {
      set_active_tab(i);  // Also rewrites every last_tab_.
    } else {
      set_active_tab(active_tab_);  // Unchecked the active tab (or re-check) -> re-latch.
    }
    break;  // set_active_tab refreshed the whole strip; stop scanning stale state.
  }

  bool en = checkbox_get(cb_enabled_);
  bool lock = checkbox_get(cb_lockmouse_);
  int vx = slider_get(sl_sensx_), vy = slider_get(sl_sensy_), vs = slider_get(sl_smooth_);

  // Only push to the settings when the user actually moved a control this frame,
  // so /rcpcam and the sliders don't fight over the live values.
  if (en != last_enabled_ || vx != last_vx_ || vy != last_vy_ || vs != last_vs_) {
    mouse_settings::set(slider_to_sens(vx), slider_to_sens(vy), slider_to_smooth(vs), en);
    update_labels();
  }
  // Cursor-lock is an independent setting (its own ini key + immediate release on
  // off), so apply it separately from the sensitivity block above.
  if (lock != last_lockmouse_) mouse_settings::set_lock_mouse(lock);
  last_enabled_ = en;
  last_lockmouse_ = lock;
  last_vx_ = vx;
  last_vy_ = vy;
  last_vs_ = vs;

  // Chase camera: push all settings only when the user moved a chase control,
  // so the /rcpchase command stays authoritative otherwise (same rule as mouse).
  bool cen = checkbox_get(cb_chase_enabled_), ccol = checkbox_get(cb_chase_collision_);
  int cd = slider_get(sl_chase_dist_);
  if (cen != last_chase_enabled_ || ccol != last_chase_collision_ || cd != last_chase_dist_) {
    chase_settings::set(cen, chase_slider_to_dist(cd), ccol);
    update_labels();
  }
  last_chase_enabled_ = cen;
  last_chase_collision_ = ccol;
  last_chase_dist_ = cd;

  // Nameplates: any checkbox change re-applies all eight (nameplate_settings::set
  // takes them positionally and refreshes the affected text/tint itself).
  bool np[kNpCount];
  bool np_changed = false;
  for (int i = 0; i < kNpCount; ++i) {
    np[i] = checkbox_get(cb_np_[i]);
    if (np[i] != last_np_[i]) np_changed = true;
  }
  if (np_changed) {
    nameplate_settings::set(np[0], np[1], np[2], np[3], np[4], np[5], np[6]);
    for (int i = 0; i < kNpCount; ++i) last_np_[i] = np[i];
  }

  // Blink speed: its own setting with its own setter (like cursor-lock).
  int bl = slider_get(sl_blink_);
  if (bl != last_blink_) {
    nameplate_settings::set_blink_ms(slider_to_blink(bl));
    update_labels();
    last_blink_ = bl;
  }

  // Color-role buttons: a click (checked-state change) opens the stock color
  // picker for that role; the button itself stays unlatched (momentary).
  for (int i = 0; i < kRoleCount; ++i) {
    bool c = checkbox_get(btn_role_[i]);
    if (c != last_role_[i]) {
      checkbox_set(btn_role_[i], false);
      last_role_[i] = false;
      if (c) open_color_picker(i);
    }
  }

  // Stock color picker session: while it is visible and was opened by us, apply
  // its RGB slider values to the selected role live (nameplates recolor as the
  // user drags). When it closes (Accept or Cancel), the last shown color stays.
  if (picker_role_ >= 0) {
    void *picker = *kColorPicker;
    char *p = static_cast<char *>(picker);
    if (!picker || !is_visible(picker) || *reinterpret_cast<void **>(p + kPickerCallerOffset) != wnd_) {
      picker_role_ = -1;
    } else {
      auto clamp255 = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
      int r = clamp255(*reinterpret_cast<int *>(p + kPickerRedOffset));
      int g = clamp255(*reinterpret_cast<int *>(p + kPickerGreenOffset));
      int b = clamp255(*reinterpret_cast<int *>(p + kPickerBlueOffset));
      int rgb = (r << 16) | (g << 8) | b;
      if (rgb != last_picker_rgb_) {
        nameplate_colors::set(picker_role_, rgb);
        set_text_color(btn_role_[picker_role_], rgb);
        last_picker_rgb_ = rgb;
      }
    }
  }
}

RcpOptionsUI::RcpOptionsUI(RcpService *rcp) {
  // No hooks: the window's SIDL templates arrive via the EQUI_OptionsWindow.xml
  // override during the client's own UI load, and the stock color picker is
  // driven by polling. This module never touches any load path.
  rcp->commands_hook->Add("/rcpoptions", {"/rcpopts"}, "Opens or closes the rof2ClientPlus options window.",
                          [this](std::vector<std::string> &args) {
                            (void)args;
                            toggle_window();
                            return true;
                          });
  logger::log("[ui] /rcpoptions registered");
}
