// rof2ClientPlus - options-window UI (stock RoF2 SIDL/EQUI port), tabbed build.
#include "rcp_options_ui.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include <string>
#include <vector>

#include "chase_cam.h"
#include "commands.h"
#include "equip_item.h"
#include "font_overlay.h"
#include "game_functions.h"
#include "logger.h"
#include "mouse_mods.h"
#include "nameplate.h"
#include "no_fog.h"
#include "rcp.h"
#include "sound_mods.h"
#include "target_ring.h"
#include "view_distance.h"

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

// CComboWnd (eqlib UI.h + offsets/eqgame.h; this eqlib build matches ours - CreateXWnd/GetChildItem/
// slider/color-picker addresses all line up). Used to drive the ring-graphic dropdown by polling.
static constexpr int kComboDeleteAll = 0x86A960;     // CComboWnd::DeleteAll()  __thiscall(this)
static constexpr int kComboInsertChoice = 0x86AE50;  // int InsertChoice(const CXStr&, uint32 data=0)
static constexpr int kComboGetCurChoice = 0x86A780;  // int GetCurChoice() const  __thiscall(this)
static constexpr int kComboSetChoice = 0x86A740;     // void SetChoice(int)  __thiscall(this, int)
// CComboWnd::pListWnd. Every method above does `mov reg,[this+0x1d8]` and derefs it with NO null
// check (disasm-verified), so calling any of them with a null list crashes. These run on the main
// thread (not rcp_guard-wrapped), so we gate on pListWnd being present.
static constexpr int kComboListWndOffset = 0x1d8;

// CListWnd (eqlib UI.h + offsets/eqgame.h; disasm-verified against OUR build: GetCurSel@0x853430 reads
// CurSel@[this+0x1f8] and compares it to the row count at [this+0x1d8] = ItemsArray.m_length, exactly as
// eqlib describes). Drives the scrollable tracked-sound list on the Sounds tab.
static constexpr int kListAddString = 0x8580C0;   // int AddString(const CXStr&, COLORREF, uint32 data, void* pTa, char* tip)
static constexpr int kListGetCurSel = 0x853430;   // int GetCurSel() const  __thiscall(this)
static constexpr int kListSetCurSel = 0x853470;   // void SetCurSel(int)  __thiscall(this, int)
static constexpr int kListSetItemText = 0x8575B0; // void SetItemText(int row, int col, const CXStr&)  __thiscall
static constexpr int kListRemoveLine = 0x8582A0;  // void RemoveLine(int)  __thiscall(this, int)
static constexpr int kListItemsCountOffset = 0x1d8;    // CListWnd::ItemsArray -> ArrayClass m_length @ +0 = row count
static constexpr int kListHeaderHeightOffset = 0x208;  // CListWnd::HeaderHeight (scrollbar-rect header inset)
// CListWnd::ListWndStyle @ this+0x274; bit 0x200000 = "has column header". Both GetItemRect (item layout /
// hit-test, 0x8551d8) and the scrollbar rect (0x8558dc) gate the header inset on this bit, and the item
// rect computes the inset height fresh (font+4) rather than from HeaderHeight - so CLEARING this bit is
// what reflows the items to the top (zeroing HeaderHeight alone only moved the scrollbar). Disasm-verified.
static constexpr int kListStyleOffset = 0x274;
static constexpr uint32_t kListStyle_ColumnHeader = 0x200000u;

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

// ---- CComboWnd wrappers (drive the ring-graphic dropdown) ----
// True only when the combo exists AND its popup list is created (see kComboListWndOffset). All the
// wrappers below no-op / return -1 unless this holds, so a half-built combo can never crash us.
static bool combo_ready(void *combo) {
  return combo && *reinterpret_cast<void **>(reinterpret_cast<char *>(combo) + kComboListWndOffset);
}
static void combo_delete_all(void *combo) {
  if (combo_ready(combo)) reinterpret_cast<void(__thiscall *)(void *)>(kComboDeleteAll)(combo);
}
static void combo_insert_choice(void *combo, const char *text) {
  if (!combo_ready(combo)) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  reinterpret_cast<int(__thiscall *)(void *, const void *, uint32_t)>(kComboInsertChoice)(combo, &cxstr, 0);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);  // InsertChoice copies; release our temp.
}
static int combo_get_cur_choice(void *combo) {
  return combo_ready(combo) ? reinterpret_cast<int(__thiscall *)(const void *)>(kComboGetCurChoice)(combo) : -1;
}
static void combo_set_choice(void *combo, int index) {
  if (combo_ready(combo)) reinterpret_cast<void(__thiscall *)(void *, int)>(kComboSetChoice)(combo, index);
}
// True while the dropdown is showing its choice list (the popup CListWnd @ combo+0x1d8 is visible), so
// callers can avoid rebuilding the choices out from under the user mid-browse.
static bool combo_popup_open(void *combo) {
  if (!combo) return false;
  void *list = *reinterpret_cast<void **>(reinterpret_cast<char *>(combo) + kComboListWndOffset);
  return list && is_visible(list);
}

// ---- CListWnd wrappers (drive the scrollable tracked-sound list). All gate on a non-null list. ----
static int list_row_count(void *list) {
  return list ? *reinterpret_cast<int *>(reinterpret_cast<char *>(list) + kListItemsCountOffset) : 0;
}
static void list_clear(void *list) {
  if (!list) return;
  for (int i = list_row_count(list) - 1; i >= 0; --i)
    reinterpret_cast<void(__thiscall *)(void *, int)>(kListRemoveLine)(list, i);
}
static int list_add_string(void *list, const char *text, uint32_t argb) {
  if (!list) return -1;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  int idx = reinterpret_cast<int(__thiscall *)(void *, const void *, uint32_t, uint32_t, void *, const char *)>(
      kListAddString)(list, &cxstr, argb, 0, nullptr, nullptr);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);  // AddString copies; release our temp.
  return idx;
}
static void list_set_item_text(void *list, int row, const char *text) {
  if (!list) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  reinterpret_cast<void(__thiscall *)(void *, int, int, const void *)>(kListSetItemText)(list, row, 0, &cxstr);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);
}
static int list_get_cur_sel(void *list) {
  return list ? reinterpret_cast<int(__thiscall *)(const void *)>(kListGetCurSel)(list) : -1;
}
static void list_set_cur_sel(void *list, int row) {
  if (list) reinterpret_cast<void(__thiscall *)(void *, int)>(kListSetCurSel)(list, row);
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

// Nameplate max-distance slider: 0..100 steps -> 0..max_dist_cap world units (step = cap/100).
static constexpr int kNpDistSliderMax = 100;
static int np_dist_to_slider(float d) {
  int v = static_cast<int>(d / (font_overlay::max_dist_cap() / kNpDistSliderMax) + 0.5f);
  return v < 0 ? 0 : (v > kNpDistSliderMax ? kNpDistSliderMax : v);
}
static float np_slider_to_dist(int v) { return v * (font_overlay::max_dist_cap() / kNpDistSliderMax); }

// Blink-speed slider: 0..56 -> 200..3000 ms full blink cycle (50 ms per step).
static constexpr int kBlinkSliderMax = 56;
static int blink_to_slider(int ms) {
  int v = (ms - 200) / 50;
  return v < 0 ? 0 : (v > kBlinkSliderMax ? kBlinkSliderMax : v);
}
static int slider_to_blink(int v) { return 200 + v * 50; }

// Target-ring radius sliders: 0..60 steps -> 0..radius_max world units (step = max/60).
static constexpr int kRingRadiusSliderMax = 60;
static int ring_radius_to_slider(float r) {
  int v = static_cast<int>(r / (target_ring_settings::radius_max() / kRingRadiusSliderMax) + 0.5f);
  return v < 0 ? 0 : (v > kRingRadiusSliderMax ? kRingRadiusSliderMax : v);
}
static float ring_slider_to_radius(int v) { return v * (target_ring_settings::radius_max() / kRingRadiusSliderMax); }
// Target-ring opacity slider: 0..100 -> 0..1.
static constexpr int kRingOpacitySliderMax = 100;
static int ring_opacity_to_slider(float a) {
  int v = static_cast<int>(a * kRingOpacitySliderMax + 0.5f);
  return v < 0 ? 0 : (v > kRingOpacitySliderMax ? kRingOpacitySliderMax : v);
}
static float ring_slider_to_opacity(int v) { return v / static_cast<float>(kRingOpacitySliderMax); }

// View-distance sliders (Display tab), shared by terrain far clip + actor clip: 0..200 steps ->
// 0..20000 world units (100 units/step). Raw 0 == the layer off (client default).
static constexpr int kViewDistSliderMax = 200;
static constexpr int kViewDistStep = 100;
static int viewdist_to_slider(int units) {
  int v = (units + kViewDistStep / 2) / kViewDistStep;
  return v < 0 ? 0 : (v > kViewDistSliderMax ? kViewDistSliderMax : v);
}
static int slider_to_viewdist(int v) { return v * kViewDistStep; }

// Sentinel "role" for the ring color button so it reuses the stock color-picker machinery (which is
// keyed on an int role) without colliding with the 0..kRoleCount-1 nameplate roles.
static constexpr int kRingColorRole = 100;

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
static const char *const kTabChildNames[] = {"Rcp_TabMouse",   "Rcp_TabCamera",  "Rcp_TabNameplate",
                                             "Rcp_TabColors", "Rcp_TabDisplay", "Rcp_TabRing",
                                             "Rcp_TabSounds"};

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
  cb_equip_ = get_child(wnd_, "Rcp_Equip");
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
  cb_np_billboard_ = get_child(wnd_, "Rcp_NpBillboard");
  cb_np_hp_ = get_child(wnd_, "Rcp_NpHpBar");
  cb_np_mana_ = get_child(wnd_, "Rcp_NpManaBar");
  cb_np_stam_ = get_child(wnd_, "Rcp_NpStamBar");
  for (int i = 0; i < kNpCount; ++i) cb_np_[i] = get_child(wnd_, kNpChildNames[i]);
  sl_blink_ = get_child(wnd_, "Rcp_BlinkSpeed");
  lbl_blink_hdr_ = get_child(wnd_, "Rcp_BlinkSpeedLabel");
  lbl_blink_ = get_child(wnd_, "Rcp_BlinkSpeedValue");
  sl_np_dist_ = get_child(wnd_, "Rcp_NpDist");
  lbl_np_dist_hdr_ = get_child(wnd_, "Rcp_NpDistLabel");
  lbl_np_dist_ = get_child(wnd_, "Rcp_NpDistValue");
  char rolename[16];
  for (int i = 0; i < kRoleCount; ++i) {
    std::snprintf(rolename, sizeof(rolename), "Rcp_Role%d", i);
    btn_role_[i] = get_child(wnd_, rolename);
  }
  cb_nofog_ = get_child(wnd_, "Rcp_NoFog");
  sl_far_ = get_child(wnd_, "Rcp_FarClip");
  lbl_far_hdr_ = get_child(wnd_, "Rcp_FarClipLabel");
  lbl_far_ = get_child(wnd_, "Rcp_FarClipValue");
  sl_actor_ = get_child(wnd_, "Rcp_ActorClip");
  lbl_actor_hdr_ = get_child(wnd_, "Rcp_ActorClipLabel");
  lbl_actor_ = get_child(wnd_, "Rcp_ActorClipValue");
  cb_ring_enabled_ = get_child(wnd_, "Rcp_RingEnabled");
  cb_ring_hideself_ = get_child(wnd_, "Rcp_RingHideSelf");
  cb_ring_concolor_ = get_child(wnd_, "Rcp_RingConColor");
  btn_ring_color_ = get_child(wnd_, "Rcp_RingColor");
  sl_ring_outer_ = get_child(wnd_, "Rcp_RingOuter");
  lbl_ring_outer_hdr_ = get_child(wnd_, "Rcp_RingOuterLabel");
  lbl_ring_outer_ = get_child(wnd_, "Rcp_RingOuterValue");
  sl_ring_inner_ = get_child(wnd_, "Rcp_RingInner");
  lbl_ring_inner_hdr_ = get_child(wnd_, "Rcp_RingInnerLabel");
  lbl_ring_inner_ = get_child(wnd_, "Rcp_RingInnerValue");
  sl_ring_opacity_ = get_child(wnd_, "Rcp_RingOpacity");
  lbl_ring_opacity_hdr_ = get_child(wnd_, "Rcp_RingOpacityLabel");
  lbl_ring_opacity_ = get_child(wnd_, "Rcp_RingOpacityValue");
  lbl_ring_graphic_hdr_ = get_child(wnd_, "Rcp_RingGraphicLabel");
  combo_ring_graphic_ = get_child(wnd_, "Rcp_RingGraphic");
  cb_ring_spin_ = get_child(wnd_, "Rcp_RingSpin");
  cb_ring_melee_ = get_child(wnd_, "Rcp_RingMelee");
  lbl_snd_add_ = get_child(wnd_, "Rcp_SndAddLabel");
  combo_snd_add_ = get_child(wnd_, "Rcp_SndAddCombo");
  lbl_snd_list_ = get_child(wnd_, "Rcp_SndListLabel");
  list_snd_ = get_child(wnd_, "Rcp_SndList");
  lbl_snd_vol_hdr_ = get_child(wnd_, "Rcp_SndVolLabel");
  sl_snd_vol_ = get_child(wnd_, "Rcp_SndVol");
  lbl_snd_vol_ = get_child(wnd_, "Rcp_SndVolValue");
  btn_snd_reset_ = get_child(wnd_, "Rcp_SndReset");
  logger::logf("[ui] controls bound: tabs=%p,%p,%p,%p mouse(en=%p sx=%p) chase(en=%p dist=%p) np0=%p role0=%p",
               btn_tab_[0], btn_tab_[1], btn_tab_[2], btn_tab_[3], cb_enabled_, sl_sensx_, cb_chase_enabled_,
               sl_chase_dist_, cb_np_[0], btn_role_[0]);

  slider_set_range(sl_sensx_, kSensSliderMax);  // 0..200 -> 0..20x sensitivity.
  slider_set_range(sl_sensy_, kSensSliderMax);
  slider_set_range(sl_smooth_, kSmoothSliderMax);         // 0..100 -> 0..0.9 smoothing.
  slider_set_range(sl_chase_dist_, kChaseDistSliderMax);  // 0..300 world units (0 = native max).
  slider_set_range(sl_blink_, kBlinkSliderMax);           // 0..56 -> 200..3000 ms blink cycle.
  slider_set_range(sl_np_dist_, kNpDistSliderMax);        // 0..100 -> 0..cap nameplate draw distance.
  slider_set_range(sl_ring_outer_, kRingRadiusSliderMax);   // 0..60 -> 0..radius_max ring outer radius.
  slider_set_range(sl_ring_inner_, kRingRadiusSliderMax);   // 0..60 -> 0..radius_max ring inner radius.
  slider_set_range(sl_ring_opacity_, kRingOpacitySliderMax);  // 0..100 -> 0..1 opacity.
  slider_set_range(sl_far_, kViewDistSliderMax);    // 0..200 -> 0..20000 world units terrain far clip.
  slider_set_range(sl_actor_, kViewDistSliderMax);  // 0..200 -> 0..20000 world units actor draw distance.
  slider_set_range(sl_snd_vol_, 300);               // 0..300 percent volume (0 = mute, 100 = unchanged, >100 boosts).

  refresh_role_tints();
  set_text_color(btn_ring_color_, target_ring_settings::get_color());  // Ring color swatch (its own color store).
  populate_graphic_combo();     // Fill the ring-graphic dropdown from disk + select the current one.
  populate_sound_add_combo();   // Fill the "add sound" dropdown from recently-played untracked sounds.
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
                   lbl_sensy_,  lbl_smooth_hdr_, sl_smooth_, lbl_smooth_, cb_lockmouse_, cb_equip_};
  void *camera[] = {cb_chase_enabled_, cb_chase_collision_, lbl_chase_dist_hdr_, sl_chase_dist_, lbl_chase_dist_};
  for (void *w : mouse) show_window(w, tab == 0);
  for (void *w : camera) show_window(w, tab == 1);
  show_window(cb_np_billboard_, tab == 2);
  show_window(cb_np_hp_, tab == 2);
  show_window(cb_np_mana_, tab == 2);
  show_window(cb_np_stam_, tab == 2);
  for (int i = 0; i < kNpCount; ++i) show_window(cb_np_[i], tab == 2);
  show_window(lbl_blink_hdr_, tab == 2);
  show_window(sl_blink_, tab == 2);
  show_window(lbl_blink_, tab == 2);
  show_window(lbl_np_dist_hdr_, tab == 2);
  show_window(sl_np_dist_, tab == 2);
  show_window(lbl_np_dist_, tab == 2);
  for (int i = 0; i < kRoleCount; ++i) show_window(btn_role_[i], tab == 3);
  void *display[] = {cb_nofog_,      lbl_far_hdr_,   sl_far_,        lbl_far_,
                     lbl_actor_hdr_, sl_actor_,      lbl_actor_};
  for (void *w : display) show_window(w, tab == 4);
  void *ring[] = {cb_ring_enabled_,   cb_ring_hideself_,    cb_ring_concolor_,  cb_ring_melee_, btn_ring_color_,
                  lbl_ring_outer_hdr_, sl_ring_outer_,       lbl_ring_outer_,
                  lbl_ring_inner_hdr_, sl_ring_inner_,       lbl_ring_inner_,
                  lbl_ring_opacity_hdr_, sl_ring_opacity_,   lbl_ring_opacity_,
                  lbl_ring_graphic_hdr_, combo_ring_graphic_, cb_ring_spin_};
  for (void *w : ring) show_window(w, tab == 5);
  void *sounds[] = {lbl_snd_add_,     combo_snd_add_, lbl_snd_list_,  list_snd_,
                    lbl_snd_vol_hdr_, sl_snd_vol_,    lbl_snd_vol_,   btn_snd_reset_};
  for (void *w : sounds) show_window(w, tab == 6);
  // Sync the list contents when the Sounds tab is entered (the CListWnd keeps its rows when hidden, so
  // this only does work when the tracked set actually changed since we last painted it).
  if (tab == 6) refresh_sound_list();
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
  last_picker_rgb_ = (role == kRingColorRole) ? target_ring_settings::get_color() : nameplate_colors::get(role);
  reinterpret_cast<int(__thiscall *)(void *, void *, uint32_t)>(kColorPickerOpen)(
      picker, wnd_, 0xFF000000u | static_cast<uint32_t>(last_picker_rgb_));
  logger::logf("[ui] color picker opened for %s",
               role == kRingColorRole ? "target ring" : nameplate_colors::name(role));
}

// Rebuild the ring-graphic dropdown from the .tga files on disk and select the current graphic.
// Called on window creation and every open (files may have changed). graphic_choices_ caches the
// index -> name mapping the on_frame poll uses to translate a selected index back to a name.
void RcpOptionsUI::populate_graphic_combo() {
  if (!combo_ring_graphic_) return;
  graphic_choices_ = target_ring_settings::get_available_graphics();  // "None" first, then *.tga stems.
  combo_delete_all(combo_ring_graphic_);
  for (const std::string &name : graphic_choices_) combo_insert_choice(combo_ring_graphic_, name.c_str());
  // Select the entry matching the persisted graphic (empty selection == "None", which is index 0).
  std::string cur = target_ring_settings::get_graphic();
  if (cur.empty()) cur = "None";
  int sel = 0;
  for (size_t i = 0; i < graphic_choices_.size(); ++i)
    if (graphic_choices_[i] == cur) {
      sel = static_cast<int>(i);
      break;
    }
  combo_set_choice(combo_ring_graphic_, sel);
  // Baseline from what the combo actually reports (== sel when ready, -1 if its list is absent), so
  // the poll never mistakes this seed for a user change.
  last_ring_graphic_choice_ = combo_get_cur_choice(combo_ring_graphic_);
}

// Fill the Sounds-tab "add a sound" dropdown from the recently-played sounds not already tracked.
// snd_add_choices_[0] is a placeholder; index i>0 maps to a display name the poll passes to add_tracked.
// Selecting the placeholder (index 0) is a no-op, so re-populating resets the combo cleanly.
void RcpOptionsUI::populate_sound_add_combo() {
  if (!combo_snd_add_) return;
  snd_add_choices_ = sound_settings::get_recent_untracked(30);
  snd_add_choices_.insert(snd_add_choices_.begin(), "-- add a sound --");
  combo_delete_all(combo_snd_add_);
  for (const std::string &name : snd_add_choices_) combo_insert_choice(combo_snd_add_, name.c_str());
  combo_set_choice(combo_snd_add_, 0);
  last_snd_add_choice_ = combo_get_cur_choice(combo_snd_add_);
}

// Repaint the tracked-sound rows (name + volume, selection highlight) and, on a selection change, sync
// the volume slider to the selected sound. Cheap when nothing changed: a signature of the list guards
// the per-row SetWindowText/Show calls so we don't repaint (or fight the user) every frame.
// Row label for a tracked sound: "name    50%" or "name    MUTED".
static std::string sound_row_text(const sound_settings::TrackedSound &t) {
  char buf[192];
  if (t.vol_pct == 0)
    std::snprintf(buf, sizeof(buf), "%s    MUTED", t.display.c_str());
  else
    std::snprintf(buf, sizeof(buf), "%s    %d%%", t.display.c_str(), t.vol_pct);
  return buf;
}

void RcpOptionsUI::refresh_sound_list() {
  if (!list_snd_) return;
  // Remove the empty column-header row (a ~1-row gap above the items). Clearing the "has column header"
  // style bit reflows the items to the top (item layout gates the header inset on it); also zero
  // HeaderHeight so the scrollbar spans the full height and nothing draws a stray header. Idempotent, so
  // we just re-assert both every refresh.
  char *lw = reinterpret_cast<char *>(list_snd_);
  *reinterpret_cast<uint32_t *>(lw + kListStyleOffset) &= ~kListStyle_ColumnHeader;
  *reinterpret_cast<int *>(lw + kListHeaderHeightOffset) = 0;
  std::vector<sound_settings::TrackedSound> tracked = sound_settings::get_tracked();  // sorted by display

  std::vector<std::string> stems, texts;
  stems.reserve(tracked.size());
  texts.reserve(tracked.size());
  for (const auto &t : tracked) {
    stems.push_back(t.stem);
    texts.push_back(sound_row_text(t));
  }

  // Drop the selection if its sound is no longer tracked (e.g. removed by a /rcpsound command).
  int sel_idx = -1;
  for (size_t i = 0; i < stems.size(); ++i)
    if (stems[i] == snd_selected_) { sel_idx = static_cast<int>(i); break; }
  if (!snd_selected_.empty() && sel_idx < 0) snd_selected_.clear();

  if (stems != snd_row_stems_) {
    // Structural change (a sound was added/removed): rebuild the whole list. Infrequent, so the scroll
    // reset is acceptable; volume-only edits take the in-place branch below and keep the scroll position.
    list_clear(list_snd_);
    for (const std::string &txt : texts) list_add_string(list_snd_, txt.c_str(), 0xFFFFFFFFu);
    snd_row_stems_ = stems;
    snd_row_texts_ = texts;
    list_set_cur_sel(list_snd_, sel_idx);  // -1 clears the selection
    last_snd_sel_row_ = sel_idx;
    snd_selection_dirty_ = true;  // re-sync the slider to the (re-found) selection
  } else {
    // Same set of sounds: update only the rows whose text changed (a volume edit) - no rebuild, so the
    // user's scroll position and selection are preserved.
    for (size_t i = 0; i < texts.size(); ++i)
      if (i >= snd_row_texts_.size() || texts[i] != snd_row_texts_[i]) {
        list_set_item_text(list_snd_, static_cast<int>(i), texts[i].c_str());
      }
    snd_row_texts_ = texts;
  }

  // On a selection change, snap the volume slider + value label to the selected sound's volume.
  if (snd_selection_dirty_) {
    snd_selection_dirty_ = false;
    int vol = -1;
    for (const auto &t : tracked)
      if (t.stem == snd_selected_) { vol = t.vol_pct; break; }
    slider_set(sl_snd_vol_, vol < 0 ? 0 : vol);
    last_snd_vol_ = vol < 0 ? 0 : vol;
    if (vol < 0) {
      set_label_text(lbl_snd_vol_, "-");
    } else {
      char vb[16];
      std::snprintf(vb, sizeof(vb), "%d%%", vol);
      set_label_text(lbl_snd_vol_, vb);
    }
  }
}

// Push current settings into the controls (called when opening).
void RcpOptionsUI::sync_controls() {
  checkbox_set(cb_enabled_, mouse_settings::get_enabled());
  checkbox_set(cb_lockmouse_, mouse_settings::get_lock_mouse());
  checkbox_set(cb_equip_, equip_item_settings::get_enabled());
  slider_set(sl_sensx_, sens_to_slider(mouse_settings::get_sens_x()));
  slider_set(sl_sensy_, sens_to_slider(mouse_settings::get_sens_y()));
  slider_set(sl_smooth_, smooth_to_slider(mouse_settings::get_smoothing()));
  checkbox_set(cb_chase_enabled_, chase_settings::get_enabled());
  checkbox_set(cb_chase_collision_, chase_settings::get_collision());
  slider_set(sl_chase_dist_, chase_dist_to_slider(chase_settings::get_max_distance()));
  checkbox_set(cb_np_billboard_, font_overlay::get_enabled());
  checkbox_set(cb_np_hp_, font_overlay::get_show_hp());
  checkbox_set(cb_np_mana_, font_overlay::get_show_mana());
  checkbox_set(cb_np_stam_, font_overlay::get_show_stam());
  bool np[kNpCount];
  np_read_settings(np);
  for (int i = 0; i < kNpCount; ++i) checkbox_set(cb_np_[i], np[i]);
  slider_set(sl_blink_, blink_to_slider(nameplate_settings::get_blink_ms()));
  slider_set(sl_np_dist_, np_dist_to_slider(font_overlay::get_max_dist()));
  checkbox_set(cb_nofog_, no_fog_settings::get_enabled());
  slider_set(sl_far_, viewdist_to_slider(view_distance_settings::get_clip()));
  slider_set(sl_actor_, viewdist_to_slider(view_distance_settings::get_actor_clip()));
  checkbox_set(cb_ring_enabled_, target_ring_settings::get_enabled());
  checkbox_set(cb_ring_hideself_, target_ring_settings::get_hide_self());
  checkbox_set(cb_ring_concolor_, target_ring_settings::get_use_con_color());
  checkbox_set(cb_ring_spin_, target_ring_settings::get_spin());
  checkbox_set(cb_ring_melee_, target_ring_settings::get_melee_range());
  slider_set(sl_ring_outer_, ring_radius_to_slider(target_ring_settings::get_outer()));
  slider_set(sl_ring_inner_, ring_radius_to_slider(target_ring_settings::get_inner()));
  slider_set(sl_ring_opacity_, ring_opacity_to_slider(target_ring_settings::get_opacity()));
  populate_graphic_combo();  // Refresh the dropdown choices + selection from disk/settings.
  populate_sound_add_combo();  // Refresh the "add sound" choices from the latest history.
  refresh_sound_list();        // Paint the tracked-sound rows + selection.
  refresh_role_tints();
  set_text_color(btn_ring_color_, target_ring_settings::get_color());
}

// Snapshot the current raw control values so the frame poll treats the sync above
// as the baseline (not a user change) and doesn't overwrite settings next frame.
void RcpOptionsUI::seed_last_values() {
  last_enabled_ = checkbox_get(cb_enabled_);
  last_lockmouse_ = checkbox_get(cb_lockmouse_);
  last_equip_ = checkbox_get(cb_equip_);
  last_vx_ = slider_get(sl_sensx_);
  last_vy_ = slider_get(sl_sensy_);
  last_vs_ = slider_get(sl_smooth_);
  last_chase_enabled_ = checkbox_get(cb_chase_enabled_);
  last_chase_collision_ = checkbox_get(cb_chase_collision_);
  last_chase_dist_ = slider_get(sl_chase_dist_);
  last_np_billboard_ = checkbox_get(cb_np_billboard_);
  last_np_hp_ = checkbox_get(cb_np_hp_);
  last_np_mana_ = checkbox_get(cb_np_mana_);
  last_np_stam_ = checkbox_get(cb_np_stam_);
  for (int i = 0; i < kNpCount; ++i) last_np_[i] = checkbox_get(cb_np_[i]);
  last_blink_ = slider_get(sl_blink_);
  last_np_dist_ = slider_get(sl_np_dist_);
  for (int i = 0; i < kTabCount; ++i) last_tab_[i] = checkbox_get(btn_tab_[i]);
  for (int i = 0; i < kRoleCount; ++i) last_role_[i] = checkbox_get(btn_role_[i]);
  last_nofog_ = checkbox_get(cb_nofog_);
  last_far_ = slider_get(sl_far_);
  last_actor_ = slider_get(sl_actor_);
  last_ring_enabled_ = checkbox_get(cb_ring_enabled_);
  last_ring_hideself_ = checkbox_get(cb_ring_hideself_);
  last_ring_concolor_ = checkbox_get(cb_ring_concolor_);
  last_ring_color_ = checkbox_get(btn_ring_color_);
  last_ring_outer_ = slider_get(sl_ring_outer_);
  last_ring_inner_ = slider_get(sl_ring_inner_);
  last_ring_opacity_ = slider_get(sl_ring_opacity_);
  last_ring_graphic_choice_ = combo_get_cur_choice(combo_ring_graphic_);
  last_ring_spin_ = checkbox_get(cb_ring_spin_);
  last_ring_melee_ = checkbox_get(cb_ring_melee_);
  last_snd_add_choice_ = combo_get_cur_choice(combo_snd_add_);
  last_snd_vol_ = slider_get(sl_snd_vol_);
  last_snd_reset_ = checkbox_get(btn_snd_reset_);
  last_snd_sel_row_ = list_get_cur_sel(list_snd_);
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
  // Nameplate draw distance ("max" at the slider top, else the world-unit value).
  if (font_overlay::get_max_dist() >= font_overlay::max_dist_cap())
    std::snprintf(buf, sizeof(buf), "max");
  else
    std::snprintf(buf, sizeof(buf), "%.0f", font_overlay::get_max_dist());
  set_label_text(lbl_np_dist_, buf);
  // View distance (Display tab): "off" at 0, else the world-unit far/actor clip.
  int far_units = view_distance_settings::get_clip();
  if (far_units > 0)
    std::snprintf(buf, sizeof(buf), "%d", far_units);
  else
    std::snprintf(buf, sizeof(buf), "off");
  set_label_text(lbl_far_, buf);
  int actor_units = view_distance_settings::get_actor_clip();
  if (actor_units > 0)
    std::snprintf(buf, sizeof(buf), "%d", actor_units);
  else
    std::snprintf(buf, sizeof(buf), "off");
  set_label_text(lbl_actor_, buf);
  // Target-ring radii + opacity.
  std::snprintf(buf, sizeof(buf), "%.1f", target_ring_settings::get_outer());
  set_label_text(lbl_ring_outer_, buf);
  std::snprintf(buf, sizeof(buf), "%.1f", target_ring_settings::get_inner());
  set_label_text(lbl_ring_inner_, buf);
  std::snprintf(buf, sizeof(buf), "%.0f%%", target_ring_settings::get_opacity() * 100.0f);
  set_label_text(lbl_ring_opacity_, buf);
  // (The ring-graphic combobox shows its own selection; nothing to refresh here.)
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
    wnd_ = cb_enabled_ = cb_lockmouse_ = cb_equip_ = sl_sensx_ = sl_sensy_ = sl_smooth_ = nullptr;
    lbl_sensx_hdr_ = lbl_sensy_hdr_ = lbl_smooth_hdr_ = lbl_sensx_ = lbl_sensy_ = lbl_smooth_ = nullptr;
    cb_chase_enabled_ = cb_chase_collision_ = sl_chase_dist_ = lbl_chase_dist_hdr_ = lbl_chase_dist_ = nullptr;
    sl_blink_ = lbl_blink_hdr_ = lbl_blink_ = nullptr;
    cb_np_billboard_ = cb_np_hp_ = cb_np_mana_ = cb_np_stam_ = nullptr;
    sl_np_dist_ = lbl_np_dist_hdr_ = lbl_np_dist_ = nullptr;
    cb_nofog_ = nullptr;
    sl_far_ = lbl_far_hdr_ = lbl_far_ = sl_actor_ = lbl_actor_hdr_ = lbl_actor_ = nullptr;
    cb_ring_enabled_ = cb_ring_hideself_ = cb_ring_concolor_ = btn_ring_color_ = nullptr;
    sl_ring_outer_ = lbl_ring_outer_hdr_ = lbl_ring_outer_ = nullptr;
    sl_ring_inner_ = lbl_ring_inner_hdr_ = lbl_ring_inner_ = nullptr;
    sl_ring_opacity_ = lbl_ring_opacity_hdr_ = lbl_ring_opacity_ = nullptr;
    lbl_ring_graphic_hdr_ = combo_ring_graphic_ = cb_ring_spin_ = cb_ring_melee_ = nullptr;
    lbl_snd_add_ = combo_snd_add_ = lbl_snd_list_ = list_snd_ = nullptr;
    lbl_snd_vol_hdr_ = sl_snd_vol_ = lbl_snd_vol_ = btn_snd_reset_ = nullptr;
    snd_row_stems_.clear();
    snd_row_texts_.clear();
    snd_add_choices_.clear();
    snd_selected_.clear();
    graphic_choices_.clear();
    last_ring_graphic_choice_ = -1;
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
  bool equip = checkbox_get(cb_equip_);
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
  // Right-click-to-equip is its own independent toggle (own module + ini key).
  if (equip != last_equip_) equip_item_settings::set_enabled(equip);
  last_enabled_ = en;
  last_lockmouse_ = lock;
  last_equip_ = equip;
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

  // Custom billboard nameplates: standalone toggle -> font_overlay (applies suppression + persists).
  bool bb = checkbox_get(cb_np_billboard_);
  if (bb != last_np_billboard_) {
    font_overlay::set_enabled(bb);
    last_np_billboard_ = bb;
  }
  // Per-bar toggles -> font_overlay::set_bars.
  bool bhp = checkbox_get(cb_np_hp_), bmana = checkbox_get(cb_np_mana_), bstam = checkbox_get(cb_np_stam_);
  if (bhp != last_np_hp_ || bmana != last_np_mana_ || bstam != last_np_stam_) {
    font_overlay::set_bars(bhp, bmana, bstam);
    last_np_hp_ = bhp;
    last_np_mana_ = bmana;
    last_np_stam_ = bstam;
  }
  // Nameplate draw distance slider.
  int nd = slider_get(sl_np_dist_);
  if (nd != last_np_dist_) {
    font_overlay::set_max_dist(np_slider_to_dist(nd));
    update_labels();
    last_np_dist_ = nd;
  }

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

  // Display: remove-distance-fog toggle -> no_fog_settings (applies next frame + persists).
  bool nf = checkbox_get(cb_nofog_);
  if (nf != last_nofog_) {
    no_fog_settings::set_enabled(nf);
    last_nofog_ = nf;
  }
  // View distance: terrain far clip + actor draw distance sliders (world units; 0 = off).
  int fc = slider_get(sl_far_);
  if (fc != last_far_) {
    view_distance_settings::set_clip(slider_to_viewdist(fc));
    update_labels();
    last_far_ = fc;
  }
  int ac = slider_get(sl_actor_);
  if (ac != last_actor_) {
    view_distance_settings::set_actor_clip(slider_to_viewdist(ac));
    update_labels();
    last_actor_ = ac;
  }

  // Ring tab: enable + hide-self checkboxes, radius/opacity sliders (same "only on real change" rule).
  bool ren = checkbox_get(cb_ring_enabled_);
  if (ren != last_ring_enabled_) {
    target_ring_settings::set_enabled(ren);
    last_ring_enabled_ = ren;
  }
  bool rhs = checkbox_get(cb_ring_hideself_);
  if (rhs != last_ring_hideself_) {
    target_ring_settings::set_hide_self(rhs);
    last_ring_hideself_ = rhs;
  }
  bool rcc = checkbox_get(cb_ring_concolor_);
  if (rcc != last_ring_concolor_) {
    target_ring_settings::set_use_con_color(rcc);
    last_ring_concolor_ = rcc;
  }
  bool rsp = checkbox_get(cb_ring_spin_);
  if (rsp != last_ring_spin_) {
    target_ring_settings::set_spin(rsp);
    last_ring_spin_ = rsp;
  }
  bool rmr = checkbox_get(cb_ring_melee_);
  if (rmr != last_ring_melee_) {
    target_ring_settings::set_melee_range(rmr);
    last_ring_melee_ = rmr;
  }
  int ro = slider_get(sl_ring_outer_);
  if (ro != last_ring_outer_) {
    target_ring_settings::set_outer(ring_slider_to_radius(ro));
    update_labels();
    last_ring_outer_ = ro;
  }
  int rin = slider_get(sl_ring_inner_);
  if (rin != last_ring_inner_) {
    target_ring_settings::set_inner(ring_slider_to_radius(rin));
    update_labels();
    last_ring_inner_ = rin;
  }
  int rop = slider_get(sl_ring_opacity_);
  if (rop != last_ring_opacity_) {
    target_ring_settings::set_opacity(ring_slider_to_opacity(rop));
    update_labels();
    last_ring_opacity_ = rop;
  }
  // Ring color swatch: momentary click opens the stock picker (sentinel role), like the role buttons.
  bool rc = checkbox_get(btn_ring_color_);
  if (rc != last_ring_color_) {
    checkbox_set(btn_ring_color_, false);
    last_ring_color_ = false;
    if (rc) open_color_picker(kRingColorRole);
  }

  // Ring graphic: native combobox. Poll its selected index; on a real change, translate the index
  // back to a name via graphic_choices_ (index 0 == "None" == the solid ring) and apply it.
  int gc = combo_get_cur_choice(combo_ring_graphic_);
  if (gc != last_ring_graphic_choice_) {
    last_ring_graphic_choice_ = gc;
    if (gc >= 0 && gc < static_cast<int>(graphic_choices_.size())) {
      const std::string &name = graphic_choices_[gc];
      target_ring_settings::set_graphic(name == "None" ? std::string() : name);
    }
  }

  // Sounds tab: manage the tracked-sound list (only while it is the active tab).
  if (active_tab_ == 6) {
    // Add-from-recent combobox: index 0 is the placeholder; any other pick starts tracking that sound.
    int add_sel = combo_get_cur_choice(combo_snd_add_);
    if (add_sel != last_snd_add_choice_) {
      last_snd_add_choice_ = add_sel;
      if (add_sel > 0 && add_sel < static_cast<int>(snd_add_choices_.size())) {
        snd_selected_ = sound_settings::add_tracked(snd_add_choices_[add_sel]);
        snd_selection_dirty_ = true;
        populate_sound_add_combo();  // now tracked -> drop from the add list + reset to the placeholder
      }
    } else if (!combo_popup_open(combo_snd_add_)) {
      // Keep the dropdown current as new sounds play - but only when the user isn't browsing it (else we
      // would rebuild the list out from under them) and only when it actually changed (avoids per-frame
      // combo churn). The placeholder-first choice list is compared as-is.
      std::vector<std::string> latest = sound_settings::get_recent_untracked(30);
      latest.insert(latest.begin(), "-- add a sound --");
      if (latest != snd_add_choices_) populate_sound_add_combo();
    }
    // Tracked list: the native CListWnd handles clicks + scrolling; poll its selected row and map it
    // back to a stem when the user picks a different row.
    int sel_row = list_get_cur_sel(list_snd_);
    if (sel_row != last_snd_sel_row_) {
      last_snd_sel_row_ = sel_row;
      if (sel_row >= 0 && sel_row < static_cast<int>(snd_row_stems_.size())) {
        snd_selected_ = snd_row_stems_[sel_row];
        snd_selection_dirty_ = true;
      }
    }
    // Volume slider -> selected sound (only on a real change, and only when something is selected).
    int sv = slider_get(sl_snd_vol_);
    if (sv != last_snd_vol_) {
      last_snd_vol_ = sv;
      if (!snd_selected_.empty()) {
        sound_settings::set_volume(snd_selected_, sv);
        char vb[16];
        std::snprintf(vb, sizeof(vb), "%d%%", sv);
        set_label_text(lbl_snd_vol_, vb);  // live value label (refresh only re-syncs it on selection change)
      }
    }
    // Reset button (momentary): stop tracking the selected sound.
    bool rst = checkbox_get(btn_snd_reset_);
    if (rst != last_snd_reset_) {
      checkbox_set(btn_snd_reset_, false);
      last_snd_reset_ = false;
      if (rst && !snd_selected_.empty()) {
        sound_settings::remove_tracked(snd_selected_);
        snd_selected_.clear();
        snd_selection_dirty_ = true;
        populate_sound_add_combo();  // untracked again -> reappears in the add list
      }
    }
    refresh_sound_list();  // repaint rows + selection + (when a selection changed) the slider
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
  if (picker_role_ != -1) {
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
        if (picker_role_ == kRingColorRole) {
          target_ring_settings::set_color(rgb);
          set_text_color(btn_ring_color_, rgb);
          // Picking a fixed color implies fixed-color mode, so the choice is actually visible.
          if (target_ring_settings::get_use_con_color()) {
            target_ring_settings::set_use_con_color(false);
            checkbox_set(cb_ring_concolor_, false);
            last_ring_concolor_ = false;
          }
        } else {
          nameplate_colors::set(picker_role_, rgb);
          set_text_color(btn_role_[picker_role_], rgb);
        }
        last_picker_rgb_ = rgb;
      }
    }
  }
}

RcpOptionsUI::RcpOptionsUI(RcpService *rcp) {
  // No hooks: the window's SIDL templates arrive via the EQUI_OptionsWindow.xml
  // override during the client's own UI load, and the stock color picker is
  // driven by polling. This module never touches any load path.
  rcp->commands_hook->Add("/rcpoptions", {"/rcpo"}, "Opens or closes the rof2ClientPlus options window.",
                          [this](std::vector<std::string> &args) {
                            (void)args;
                            toggle_window();
                            return true;
                          });
  logger::log("[ui] /rcpoptions registered");
}
