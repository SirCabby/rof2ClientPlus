// rof2ClientPlus - options-window UI (stock RoF2 SIDL/EQUI port), full build.
#include "rcp_options_ui.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "commands.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "logger.h"
#include "mouse_mods.h"
#include "rcp.h"

// ---- stock RoF2 addresses (eqlib offsets + disasm of the client's own usage) ----
static constexpr int kLoadSidl = 0x870C60;                // CSidlManagerBase::LoadSidl(Path,Def,File,DefClient)
static constexpr int kLoadSidlCallSite = 0x496228;        // the in-game UI loader's LoadSidl("EQUI.xml") call (e8)
static constexpr int kCreateXWnd = 0x870400;              // CSidlManagerBase::CreateXWndFromTemplate(parent,name)
static constexpr int kGetChildItem = 0x85CFD0;            // CSidlScreenWnd::GetChildItem(name, flag)
static constexpr int kSliderGetValue = 0x895FE0;          // CSliderWnd::GetValue()
static constexpr int kSliderSetValue = 0x8961B0;          // CSliderWnd::SetValue(int)
static constexpr int kCXStrCtor = 0x805C20;               // CXStr::CXStr(const char*)
static void **const kSidlManager = reinterpret_cast<void **>(0x15D3D08);  // pinstCSidlManager
static constexpr int kShowVtOffset = 0xD8;                // CXWnd vtable Show() slot
static constexpr int kCheckedOffset = 0x1E4;             // CButtonWnd::Checked
static constexpr int kDShowOffset = 0x196;               // CXWnd::dShow (visible flag)

// ---- thin wrappers over the client's __thiscall methods ----

// Stock CXStr = { CStrRep* m_data }; CStrRep->utf8 chars at +0x14. LoadSidl's
// CXStr args are passed by const ref, so each arg is a CXStr* (guarded read).
static const char *cxstr_chars(void *cxstr_ref) {
  if (!cxstr_ref) return nullptr;
  uintptr_t rep = *reinterpret_cast<uintptr_t *>(cxstr_ref);
  if (rep < 0x10000 || rep > 0x7fffffff) return nullptr;
  return reinterpret_cast<char *>(rep + 0x14);
}

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

// ---- LoadSidl hook: merge our window <Include> into the client's EQUI.xml ----
namespace fs = std::filesystem;

// Copies the active EQUI.xml, inserting our <Include> before </Composite>, so the
// client's own loader parses and registers our window. Returns true on success.
static bool write_merged_equi(const fs::path &src_equi, const fs::path &merged) {
  std::ifstream in(src_equi);
  if (!in) return false;
  std::string content, line;
  bool inserted = false;
  while (std::getline(in, line)) {
    if (!inserted && line.find("</Composite>") != std::string::npos) {
      content += "  <Include>EQUI_RcpOptions.xml</Include>\n";
      inserted = true;
    }
    content += line + "\n";
  }
  in.close();
  if (!inserted) return false;
  std::ofstream out(merged);
  if (!out) return false;
  out << content;
  return true;
}

typedef void(__fastcall *LoadSidl_t)(void *, int, void *, void *, void *, void *);
static LoadSidl_t g_orig_loadsidl = nullptr;

static void __fastcall LoadSidl_hk(void *self, int edx, void *path, void *defpath, void *filename, void *defclient) {
  LoadSidl_t orig = g_orig_loadsidl;  // Captured at install (main thread); never reads the hook map here.
  const char *fname = cxstr_chars(filename);
  if (!fname || _stricmp(fname, "EQUI.xml") != 0) {  // Only the master UI file.
    orig(self, edx, path, defpath, filename, defclient);
    return;
  }

  const char *apath = cxstr_chars(path);
  const char *dpath = cxstr_chars(defpath);
  std::error_code ec;
  fs::path active_dir = apath ? apath : "";
  fs::path src = active_dir / "EQUI.xml";
  if (!fs::exists(src, ec) && dpath) src = fs::path(dpath) / "EQUI.xml";
  fs::path merged = active_dir / "EQUI_Rcp.xml";
  fs::path opt_dst = active_dir / "EQUI_RcpOptions.xml";
  fs::path tab_dst = active_dir / "EQUI_Tab_Cam.xml";

  // The merged EQUI carries <Include>EQUI_RcpOptions.xml</Include> (which itself
  // references EQUI_Tab_Cam.xml), so both must sit next to the merged file for
  // the client to resolve them. If a partial install left either source out of
  // uifiles/rcp, skip the merge and load EQUI.xml unmodified -- otherwise the
  // client hits the dangling <Include> and logs "file not found / Error reading
  // XML" in UIErrors.txt at login.
  const fs::path opt_src = "uifiles/rcp/EQUI_RcpOptions.xml";
  const fs::path tab_src = "uifiles/rcp/EQUI_Tab_Cam.xml";
  if (!fs::exists(opt_src, ec) || !fs::exists(tab_src, ec)) {
    logger::logf("[ui] rcp xml source missing (%s / %s); loading EQUI.xml unmodified",
                 opt_src.string().c_str(), tab_src.string().c_str());
    orig(self, edx, path, defpath, filename, defclient);
    return;
  }

  // When the active UI skin IS our uifiles/rcp folder (the user selected "rcp"
  // as their skin), active_dir == the source folder, so opt_dst/tab_dst ARE the
  // source files. Copying would be a self-copy and -- fatally -- the cleanup
  // below would delete our own EQUI_RcpOptions.xml / EQUI_Tab_Cam.xml out of
  // uifiles/rcp, breaking every subsequent login. Detect that (same file per
  // fs::equivalent, robust to case/separators under Wine) and neither copy nor
  // remove them: the sources are already in place for the client to load.
  std::error_code same_ec;
  const bool active_is_rcp_src = fs::equivalent(active_dir, "uifiles/rcp", same_ec) && !same_ec;

  if (write_merged_equi(src, merged)) {
    if (!active_is_rcp_src) {
      std::error_code opt_ec, tab_ec;
      fs::copy_file(opt_src, opt_dst, fs::copy_options::overwrite_existing, opt_ec);
      fs::copy_file(tab_src, tab_dst, fs::copy_options::overwrite_existing, tab_ec);
      if (opt_ec || tab_ec) {  // Source vanished mid-load, or perms/disk issue.
        logger::logf("[ui] rcp xml copy failed (opt=%s tab=%s); loading EQUI.xml unmodified",
                     opt_ec.message().c_str(), tab_ec.message().c_str());
        fs::remove(merged, ec);
        fs::remove(opt_dst, ec);
        fs::remove(tab_dst, ec);
        orig(self, edx, path, defpath, filename, defclient);
        return;
      }
    }
    logger::logf("[ui] injected EQUI_Rcp.xml (from %s)%s", src.string().c_str(),
                 active_is_rcp_src ? " [rcp skin: sources in place]" : "");
    uint32_t new_fn;
    cxstr_init(&new_fn, "EQUI_Rcp.xml");
    orig(self, edx, path, defpath, &new_fn, defclient);  // Client loads the merged file.
    fs::remove(merged, ec);
    if (!active_is_rcp_src) {  // Never delete the sources when they live in uifiles/rcp.
      fs::remove(opt_dst, ec);
      fs::remove(tab_dst, ec);
    }
  } else {
    logger::logf("[ui] EQUI merge failed (src=%s); loading unmodified", src.string().c_str());
    orig(self, edx, path, defpath, filename, defclient);
  }
}

// ---- RcpOptionsUI ----

void RcpOptionsUI::create_window() {
  create_attempted_ = true;
  void *sidlmgr = *kSidlManager;
  if (!sidlmgr) {
    logger::log("[ui] CreateWindow: SIDL manager is null");
    return;
  }
  uint32_t name_cxstr;
  cxstr_init(&name_cxstr, "RcpOptions");
  wnd_ = reinterpret_cast<void *(__thiscall *)(void *, void *, void *)>(kCreateXWnd)(sidlmgr, nullptr, &name_cxstr);
  logger::logf("[ui] CreateXWndFromTemplate('RcpOptions') = %p", wnd_);
  if (!wnd_) return;

  cb_enabled_ = get_child(wnd_, "Rcp_Enabled");
  cb_lockmouse_ = get_child(wnd_, "Rcp_LockMouse");
  sl_sensx_ = get_child(wnd_, "Rcp_SensX");
  sl_sensy_ = get_child(wnd_, "Rcp_SensY");
  sl_smooth_ = get_child(wnd_, "Rcp_Smooth");
  lbl_sensx_ = get_child(wnd_, "Rcp_SensXValue");
  lbl_sensy_ = get_child(wnd_, "Rcp_SensYValue");
  lbl_smooth_ = get_child(wnd_, "Rcp_SmoothValue");
  logger::logf("[ui] controls: enabled=%p lockmouse=%p sensx=%p sensy=%p smooth=%p | lbls=%p,%p,%p", cb_enabled_,
               cb_lockmouse_, sl_sensx_, sl_sensy_, sl_smooth_, lbl_sensx_, lbl_sensy_, lbl_smooth_);

  slider_set_range(sl_sensx_, kSensSliderMax);  // 0..200 -> 0..20x sensitivity.
  slider_set_range(sl_sensy_, kSensSliderMax);
  slider_set_range(sl_smooth_, kSmoothSliderMax);  // 0..100 -> 0..0.9 smoothing.

  show_window(wnd_, false);  // Created hidden; /rcpoptions reveals it.
}

// Push current settings into the controls (called when opening).
static void sync_controls_from_settings(void *cb, void *lock, void *sx, void *sy, void *sm) {
  checkbox_set(cb, mouse_settings::get_enabled());
  checkbox_set(lock, mouse_settings::get_lock_mouse());
  slider_set(sx, sens_to_slider(mouse_settings::get_sens_x()));
  slider_set(sy, sens_to_slider(mouse_settings::get_sens_y()));
  slider_set(sm, smooth_to_slider(mouse_settings::get_smoothing()));
}

void RcpOptionsUI::update_labels() {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f", mouse_settings::get_sens_x());
  set_label_text(lbl_sensx_, buf);
  std::snprintf(buf, sizeof(buf), "%.1f", mouse_settings::get_sens_y());
  set_label_text(lbl_sensy_, buf);
  std::snprintf(buf, sizeof(buf), "%.2f", mouse_settings::get_smoothing());
  set_label_text(lbl_smooth_, buf);
}

void RcpOptionsUI::toggle_window() {
  if (!wnd_ && !create_attempted_) create_window();
  if (!wnd_) {
    Rcp::Game::print_chat("rof2ClientPlus options window unavailable (is uifiles/rcp installed?).");
    return;
  }
  bool make_visible = !is_visible(wnd_);
  if (make_visible) {
    sync_controls_from_settings(cb_enabled_, cb_lockmouse_, sl_sensx_, sl_sensy_, sl_smooth_);
    update_labels();
    // Seed the last-seen values so the poll doesn't treat this sync as a user
    // change and overwrite the settings on the next frame.
    last_enabled_ = checkbox_get(cb_enabled_);
    last_lockmouse_ = checkbox_get(cb_lockmouse_);
    last_vx_ = slider_get(sl_sensx_);
    last_vy_ = slider_get(sl_sensy_);
    last_vs_ = slider_get(sl_smooth_);
  }
  show_window(wnd_, make_visible);
}

void RcpOptionsUI::on_frame() {
  // Only ever touch the window while in-game. When not in-game (login, char
  // select, zoning) the client tears its UI down, so drop our handles to avoid
  // ever dereferencing a destroyed window; they are rebuilt on next /rcpoptions.
  if (!Rcp::Game::is_in_game()) {
    wnd_ = cb_enabled_ = cb_lockmouse_ = sl_sensx_ = sl_sensy_ = sl_smooth_ = nullptr;
    create_attempted_ = false;
    return;
  }
  // Poll whenever the window handle exists (in-game already gated above). We do
  // NOT gate on is_visible: a hidden window's controls simply don't change, so
  // polling them is harmless, and it avoids depending on the visibility flag.
  if (!wnd_) return;

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
}

RcpOptionsUI::RcpOptionsUI(RcpService *rcp) {
  // Hook the specific EQUI.xml LoadSidl CALL SITE (the in-game UI loader at
  // 0x496228), not LoadSidl globally. That call site runs only in-game (world
  // entry / UI reload), so we catch the injection there and fire during the
  // world-load - while NEVER touching the char-select UI-load path (hooking
  // LoadSidl globally corrupted at char select).
  rcp->hooks->Add("ui_loadsidl", kLoadSidlCallSite, LoadSidl_hk, hook_type_replace_call);
  g_orig_loadsidl = reinterpret_cast<LoadSidl_t>(rcp->hooks->hook_map["ui_loadsidl"]->original(LoadSidl_hk));
  logger::logf("[ui] LoadSidl call-site hook installed (0x%x)", kLoadSidlCallSite);

  rcp->commands_hook->Add("/rcpoptions", {"/rcpopts"}, "Opens or closes the rof2ClientPlus options window.",
                          [this](std::vector<std::string> &args) {
                            toggle_window();
                            return true;
                          });
  logger::log("[ui] /rcpoptions registered");
}
