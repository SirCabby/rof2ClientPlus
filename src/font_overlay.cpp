// rof2ClientPlus - custom-font overlay implementation. See font_overlay.h.
#include "font_overlay.h"

#include <d3d9.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "bitmap_font.h"
#include "commands.h"
#include "crash_handler.h"
#include "directx.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "nameplate.h"
#include "rcp.h"

namespace {

constexpr char kIniSection[] = "Font";

// N4b validation state. The font is created lazily on the render thread against the
// live device the first frame the test is enabled.
bool g_test_enabled = false;
std::unique_ptr<BitmapFont> g_test_font;

// N4c de-risk: a single 3-D billboard string at the controlled player's head. This
// resolves two unknowns in one look - the entity->render-space position mapping, and
// whether the world VIEW/PROJECTION matrices are still live at EndScene time.
bool g_test3d_enabled = false;
std::unique_ptr<SpriteFont> g_test3d_font;
float g_test3d_offset_lines = 1.0f;  // Screen-up lift above the head, in text line-heights (live-tunable).

// Controlled player (self / mount / charm) and its world position floats. Same offsets
// the working chase_cam uses; the three floats are consumed by the renderer in memory
// order (0x64,0x68,0x6c), with 0x6c the vertical (EQ Z, up).
void **const kControlled = reinterpret_cast<void **>(0xDD2644);
void **const kSelf = reinterpret_cast<void **>(0xDD2630);  // local player (for mana/stamina).
// CDisplay::cameraType - the LIVE render camera (0 = first person). Same address mouse_mods and
// chase_cam read; distinct from camera_mods' selected-view slot at 0x63BE68 (which does not track
// a native zoom into first person). Use the live value so the self plate is suppressed exactly
// when the camera is actually first person.
int *const kCameraType = reinterpret_cast<int *>(0xD1FD9C);
constexpr int kEntPos0 = 0x64;         // first float the renderer reads (EQ Y).
constexpr int kEntPos1 = 0x68;         // second (EQ X).
constexpr int kEntPos2 = 0x6c;         // third, vertical (EQ Z).
constexpr int kEntAvatarHeight = 0x138;  // head height above the feet base.
constexpr int kEntType = 0x125;        // uint8: 0=Player, 1=NPC, 2=Corpse.
constexpr int kEntActor = 0x101c;      // CActor*; NULL => not in-world / no graphics yet.
constexpr int kEntHPCurrent = 0x2e4;   // for NPCs/target this IS the percent (HPMax==0).
constexpr int kEntHPMax = 0x2dc;
constexpr uint8_t kTypeCorpse = 2;

// Native nameplate anchor (research): the client attaches the name sprite to the model's
// "HEAD_NAME" skeletal point, which scales per model - so feet+AvatarHeight was wrong for NPCs.
// The live world position is on the CStringSprite at actor+0x204, world pos @ ss+0x6c/0x70/0x74
// (0x74 vertical). This is exactly where the client draws the native name.
constexpr int kActorStringSprite = 0x204;  // CStringSprite* (name sprite); NULL until a name is set.
constexpr int kSsWorldPos = 0x6c;          // head-anchor world pos (0x6c, 0x70, 0x74).

// Spawn list (research Q1, triple-confirmed): manager @ 0xE641D0; first node @ mgr+0x08;
// next node @ entity+0x08; NULL-terminated.
void **const kSpawnManager = reinterpret_cast<void **>(0xE641D0);
constexpr int kListFirst = 0x08;
constexpr int kEntNext = 0x08;

// Local-player HP/mana/endurance for the self bars (research Q5): the spawn fields are the
// target/NPC percent path and read 0 for self, so the real values live on CharacterZoneClient
// (this = pLocalPC + 0x2DC8), read via the client's own accessor functions.
void **const kLocalPC = reinterpret_cast<void **>(0xDD261C);
constexpr int kCzcOffset = 0x2DC8;
typedef int(__thiscall *StatFn2)(void *, int, bool);  // Cur_HP(0x449E00), Max_HP(0x443FA0): (0, true)
typedef int(__thiscall *StatFn1b)(void *, bool);       // Cur_Mana(0x4442E0): (true)
typedef int(__thiscall *StatFn1i)(void *, int);        // Max_Mana(0x581E60), Max_Endurance(0x582020): (0)
constexpr uintptr_t kCurHP = 0x449E00, kMaxHP = 0x443FA0, kCurMana = 0x4442E0, kMaxMana = 0x581E60;
constexpr uintptr_t kMaxEnd = 0x582020, kGetProfile = 0x7DB210;
// Current endurance has no accessor function; read it straight from PcProfile+0x3390. The profile
// pointer needs a virtual-base adjustment - reproduce exactly what the (working) Cur_Mana at
// 0x4442E0 does: arg = czc + *(*(czc+4)+4) + 8, then GetCurrentProfile(arg) (disasm 0x444323..).
constexpr int kProfileEndCurrent = 0x3390;

float g_np_offset_lines = 1.0f;  // Screen-up lift above the HEAD_NAME anchor, in text line-heights (tunable).
// Max distance to draw a plate. Effectively uncapped by default so any visible NPC keeps its plate;
// the 3D billboard is z-tested, so ones behind geometry are occluded and off-screen ones aren't
// visible. Tunable via '/rcpfont dist <n>' and the options-window slider (0 = the kNpMaxDistCap top).
constexpr float kNpMaxDistCap = 5000.f;  // Slider top; also the "effectively no cap" default.
float g_np_max_dist = kNpMaxDistCap;
float g_np_scale = 0.032f;       // Billboard world scale (shrinks with distance, like native). '/rcpfont scale <n>'.
bool g_show_hp = true;           // Per-bar toggles (HP for all entities; mana/stamina self only).
bool g_show_mana = true;
bool g_show_stam = true;

// The real billboard-nameplate feature ('/rcpfont on'): each EndScene it walks the spawn list and
// draws a custom-font plate (name + colored HP bar; +mana/stamina for self) per visible PC/NPC.
bool g_np_enabled = false;
std::unique_ptr<SpriteFont> g_np_font;

// Clamps a current/max pair to 0..100. NPCs carry the percent directly in "current" with max == 0,
// so fall back to clamping current in that case.
int to_percent(int cur, int mx) {
  if (mx > 0) {
    long long p = (long long)cur * 100 / mx;
    return p < 0 ? 0 : (p > 100 ? 100 : (int)p);
  }
  return cur < 0 ? 0 : (cur > 100 ? 100 : cur);
}

// Fills 0..100 percents for the local player's HP/mana/endurance from the character profile
// (see Q5). Returns false (and leaves outputs untouched) if the profile isn't available; each
// bar is skipped individually when its max reads <= 0 so we never draw a bogus empty bar.
int g_stat_log = 0;  // Log the raw self-stat reads for the first few frames after '/rcpfont on'.

bool self_stats(int &hp, int &mana, int &stam) {
  char *pc = *reinterpret_cast<char **>(kLocalPC);
  if (!pc) return false;
  void *czc = pc + kCzcOffset;
  const int curHP = reinterpret_cast<StatFn2>(kCurHP)(czc, 0, true);
  const int maxHP = reinterpret_cast<StatFn2>(kMaxHP)(czc, 0, true);
  const int curMana = reinterpret_cast<StatFn1b>(kCurMana)(czc, true);
  const int maxMana = reinterpret_cast<StatFn1i>(kMaxMana)(czc, 0);
  const int maxEnd = reinterpret_cast<StatFn1i>(kMaxEnd)(czc, 0);
  // Profile pointer via the virtual-base adjustment Cur_Mana uses (czc + *(*(czc+4)+4) + 8).
  int curEnd = 0;
  void *profile = nullptr;
  char *czb = static_cast<char *>(czc);
  void *p1 = *reinterpret_cast<void **>(czb + 4);
  if (p1) {
    const int vbase = *reinterpret_cast<int *>(static_cast<char *>(p1) + 4);
    profile = reinterpret_cast<void *(__thiscall *)(void *)>(kGetProfile)(czb + vbase + 8);
    if (profile) curEnd = *reinterpret_cast<int *>(static_cast<char *>(profile) + kProfileEndCurrent);
  }
  if (g_stat_log > 0) {
    logger::logf("[font] self stats: HP %d/%d  Mana %d/%d  End %d/%d (profile=%p)", curHP, maxHP, curMana, maxMana,
                 curEnd, maxEnd, profile);
    --g_stat_log;
  }
  hp = maxHP > 0 ? to_percent(curHP, maxHP) : -1;
  mana = maxMana > 0 ? to_percent(curMana, maxMana) : -1;
  stam = maxEnd > 0 ? to_percent(curEnd, maxEnd) : -1;
  return true;
}

// Persists the billboard-nameplate settings to the ini.
void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "Billboard", g_np_enabled);
  ini.setValue<float>(kIniSection, "Offset", g_np_offset_lines);
  ini.setValue<float>(kIniSection, "Scale", g_np_scale);
  ini.setValue<float>(kIniSection, "MaxDist", g_np_max_dist);
  ini.setValue<bool>(kIniSection, "HpBar", g_show_hp);
  ini.setValue<bool>(kIniSection, "ManaBar", g_show_mana);
  ini.setValue<bool>(kIniSection, "StamBar", g_show_stam);
}

// Applies the on/off state to native-nameplate suppression and persists the settings.
void apply_and_save() {
  nameplate::set_suppress_native(g_np_enabled);
  save_settings();
}

// Render callback (runs on the render thread inside directx's EndScene hook, already
// wrapped in rcp_guard). Draws a fixed 2-D screen string to prove the engine renders.
void on_render(IDirect3DDevice9 *device) {
  if (!g_test_enabled || !device) {
    if (!g_test_enabled) g_test_font.reset();  // Free resources when the test is off.
    return;
  }

  if (!g_test_font) {
    g_test_font = BitmapFont::create_bitmap_font(*device, BitmapFontBase::kDefaultFontName);
    if (!g_test_font) {
      logger::log("[font] test: failed to create BitmapFont; disabling test");
      g_test_enabled = false;
      return;
    }
    g_test_font->set_drop_shadow(true);
    logger::log("[font] test: BitmapFont created");
  }

  // Upper-left, not centered: place the text at a fixed screen position.
  g_test_font->queue_string("rof2ClientPlus font test - abcdefg 0123456789", Vec3(20, 40, 0),
                            /*center=*/false, D3DCOLOR_XRGB(255, 255, 0));
  g_test_font->flush_queue_to_screen();
}

// N4c de-risk: draw one 3-D billboard string at the controlled player's head.
void on_render_3d(IDirect3DDevice9 *device) {
  if (!g_test3d_enabled || !device) {
    if (!g_test3d_enabled) g_test3d_font.reset();
    return;
  }

  if (!g_test3d_font) {
    // A proper-sized atlas (shipped in uifiles/rcp/fonts) drawn small looks crisp; upscaling the
    // tiny embedded arial_8 was the "ugly" (blocky/blurry). arial_bold_24 is Zeal's own default.
    g_test3d_font = SpriteFont::create_sprite_font(*device, "arial_bold_24");
    if (!g_test3d_font) {
      logger::log("[font] test3d: failed to create SpriteFont; disabling test");
      g_test3d_enabled = false;
      return;
    }
    g_test3d_font->set_drop_shadow(true);
    g_test3d_font->set_scale_factor(0.025f);  // Zeal's tuned factor for arial_bold_24.
    logger::log("[font] test3d: SpriteFont created (arial_bold_24)");
  }

  void *controlled = *kControlled;
  if (!controlled) return;  // Not in-world yet.
  char *e = static_cast<char *>(controlled);
  const float p0 = *reinterpret_cast<float *>(e + kEntPos0);
  const float p1 = *reinterpret_cast<float *>(e + kEntPos1);
  const float p2 = *reinterpret_cast<float *>(e + kEntPos2);
  const float height = *reinterpret_cast<float *>(e + kEntAvatarHeight);

  // Anchor at the top of the head (no extra world-Z); the "above the head" gap is a screen-up
  // (billboard-local) lift so it stays a constant distance up-screen from any camera angle.
  Vec3 head(p0, p1, p2 + height);
  g_test3d_font->set_screen_offset(g_test3d_font->get_line_spacing() * g_test3d_offset_lines);
  g_test3d_font->queue_string("RCP 3D nameplate test", head, /*center=*/true, D3DCOLOR_XRGB(0, 255, 128));
  g_test3d_font->flush_queue_to_screen();
}

// The real billboard-nameplate render callback. Walks the client's spawn list each frame and
// draws a 3-D billboard per visible PC/NPC: the con/state-colored name (reusing nameplate.cpp's
// text + color logic) + a colored HP bar, plus mana/stamina bars for the local player. Runs on
// the render thread inside directx's rcp_guard-wrapped EndScene.
void on_render_nameplates(IDirect3DDevice9 *device) {
  if (!g_np_enabled || !device) {
    if (!g_np_enabled) g_np_font.reset();
    return;
  }
  // Only draw in-world: at login / character-select / zoning the spawn list holds the character-
  // select models (and self is bogus), so plates would wrongly render there.
  if (!Rcp::Game::is_in_game()) return;

  if (!g_np_font) {
    g_np_font = SpriteFont::create_sprite_font(*device, "arial_bold_24");
    if (!g_np_font) {
      logger::log("[font] nameplates: failed to create SpriteFont; disabling");
      g_np_enabled = false;
      return;
    }
    g_np_font->set_drop_shadow(true);
    g_np_font->set_align_bottom(true);  // Grow the plate UPWARD from the anchor so adding bars
                                        // never pushes the name down onto the head.
    logger::log("[font] nameplates: SpriteFont created");
  }
  // Normal world-scaled billboard: it sizes dynamically with distance (native does the same, with
  // clamps). A fixed world scale avoids the fragile camera-distance math that made far plates giant.
  g_np_font->set_scale_factor(g_np_scale);
  g_np_font->set_screen_offset(g_np_font->get_line_spacing() * g_np_offset_lines);

  void *mgr = *kSpawnManager;
  void *self = *kSelf;
  if (!mgr || !self) return;
  char *s = static_cast<char *>(self);
  const float s0 = *reinterpret_cast<float *>(s + kEntPos0);
  const float s1 = *reinterpret_cast<float *>(s + kEntPos1);
  const float s2 = *reinterpret_cast<float *>(s + kEntPos2);
  const float max_dist_sq = g_np_max_dist * g_np_max_dist;

  const char kBg = BitmapFontBase::kStatsBarBackground;
  const char kHp = BitmapFontBase::kHealthBarValue;
  const char kMana = BitmapFontBase::kManaBarValue;
  const char kStam = BitmapFontBase::kStaminaBarValue;

  int self_hp = -1, self_mana = -1, self_stam = -1;
  self_stats(self_hp, self_mana, self_stam);  // Once per frame (the local player's real pools).

  // In first-person view the client neither draws nor updates your own name sprite (you're inside
  // your own head), so a self billboard would freeze at the last head position and appear to trail
  // behind as you move. Suppress the self plate whenever the camera is actually first person.
  const bool first_person = (*kCameraType == 0);

  for (void *e = *reinterpret_cast<void **>(static_cast<char *>(mgr) + kListFirst); e;
       e = *reinterpret_cast<void **>(static_cast<char *>(e) + kEntNext)) {
    char *ent = static_cast<char *>(e);
    if (e == self && first_person) continue;                             // No self nameplate in first person.
    const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
    if (type == kTypeCorpse) continue;                                   // Skip corpses.
    void *actor = *reinterpret_cast<void **>(ent + kEntActor);
    if (!actor) continue;                                                // No graphics actor => not in-world.

    const float p0 = *reinterpret_cast<float *>(ent + kEntPos0);
    const float p1 = *reinterpret_cast<float *>(ent + kEntPos1);
    const float p2 = *reinterpret_cast<float *>(ent + kEntPos2);
    const float d0 = p0 - s0, d1 = p1 - s1, d2 = p2 - s2;
    if (d0 * d0 + d1 * d1 + d2 * d2 > max_dist_sq) continue;             // Distance cull (feet pos).

    // Head anchor: the model's HEAD_NAME point (native's own anchor; scales per model), read live
    // off the name sprite. Fall back to feet+AvatarHeight if the sprite doesn't exist yet.
    float h0, h1, h2;
    void *ss = *reinterpret_cast<void **>(static_cast<char *>(actor) + kActorStringSprite);
    if (ss) {
      h0 = *reinterpret_cast<float *>(static_cast<char *>(ss) + kSsWorldPos + 0);
      h1 = *reinterpret_cast<float *>(static_cast<char *>(ss) + kSsWorldPos + 4);
      h2 = *reinterpret_cast<float *>(static_cast<char *>(ss) + kSsWorldPos + 8);
    } else {
      h0 = p0;
      h1 = p1;
      h2 = p2 + *reinterpret_cast<float *>(ent + kEntAvatarHeight);
    }

    std::string text = nameplate::billboard_text(e);
    if (text.empty()) continue;
    const int rgb = nameplate::billboard_color(e);

    // Health bar. Self comes from the profile; others from the spawn percent field. Only draw
    // when > 0 (0 usually means "HP unknown" for a non-grouped player, not truly dead).
    const int hp = (e == self) ? self_hp
                               : to_percent(*reinterpret_cast<int *>(ent + kEntHPCurrent),
                                            *reinterpret_cast<int *>(ent + kEntHPMax));
    if (g_show_hp && hp > 0) {
      g_np_font->set_hp_percent(hp);
      text += '\n';
      text += kBg;
      text += kHp;
    }
    // Mana + stamina bars only for the local player (others' pools aren't known to the client).
    if (e == self) {
      if (g_show_mana && self_mana >= 0) {
        g_np_font->set_mana_percent(self_mana);
        text += '\n';
        text += kBg;
        text += kMana;
      }
      if (g_show_stam && self_stam >= 0) {
        g_np_font->set_stamina_percent(self_stam);
        text += '\n';
        text += kBg;
        text += kStam;
      }
    }

    Vec3 head(h0, h1, h2);
    const D3DCOLOR color = D3DCOLOR_XRGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    g_np_font->queue_string(text.c_str(), head, /*center=*/true, color);
  }

  // (The app-level depth-mask UI-occlusion approach was abandoned: the CXWndManager top-level list
  // is ~777 flat entries, not clean opaque panels, so it can't represent the UI. UI occlusion is
  // instead handled by drawing in the scene pass - see the in-scene render path.)
  g_np_font->flush_queue_to_screen();
}

// In-scene draw seam (research-confirmed): the client's own render callback, registered via
// CRenderInterface::SetRenderCallback (pRender vtable index 50), fires INSIDE RenderScene - after
// the world geometry (so plates z-test against the world) but BEFORE the deferred 2D UI raster (so
// the UI paints over the plates). This is exactly where the native name-sprites draw, so drawing
// here gives correct UI occlusion for free, without any window enumeration or depth tricks.
void **const kRenderInterface = reinterpret_cast<void **>(0x15D46A4);  // pinstRenderInterface
constexpr int kRenderDeviceOffset = 0xF08;         // CRender::pD3DDevice (IDirect3DDevice9*)
constexpr int kSetRenderCallbackVtblIndex = 50;    // pRender vtable +0xC8
typedef void(__cdecl *RenderCallbackPtr)();
typedef RenderCallbackPtr(__thiscall *SetRenderCallbackFn)(void *self, RenderCallbackPtr cb);
static RenderCallbackPtr g_prev_render_cb = nullptr;
static bool g_render_cb_registered = false;

// IMPORTANT ordering fact (verified by disassembly of CRender::RenderScene 0x10097420): the
// client's render callback (0x10097733) fires BEFORE the world raster - the world/actor draws and
// particles follow it, and the deferred-2D/UI raster C2DPrimitiveManager::Render (DLL RVA 0xAE370,
// __thiscall, 2 dword args, called twice per scene at 0x10097817 and 0x100978bb) comes after all
// 3D. Drawing at the callback made the world overdraw the text (invisible with z-write off) or
// z-fail behind the billboards' written depth (fog-colored blobs with z-write on). So the callback
// is used ONLY as a frame marker, and the actual draw happens at the FIRST
// C2DPrimitiveManager::Render call of the marked frame: after the full world (depth buffer
// populated -> wall occlusion correct), before the UI rasterizes (windows paint over the plates).
bool g_np_draw_pending = false;

// Extra world-space overlays registered via font_overlay::add_scene_draw (e.g. the target ring).
// Drawn each marked frame at this same seam, right after the billboard nameplates. Mutated only at
// load (single-threaded); read on the render thread, so no lock needed - like directx's callback list.
std::vector<std::function<void(IDirect3DDevice9 *)>> *g_scene_draws = nullptr;

void __cdecl scene_render_cb() {
  if (g_prev_render_cb) g_prev_render_cb();  // Preserve any previously-registered callback (always).
  if (crash_handler::shutting_down()) return;  // Don't arm a draw during teardown.
  g_np_draw_pending = true;  // Marks "a real 3D scene is being rendered this frame" (not RenderBlind).
}

constexpr uintptr_t kGfxPreferredBase = 0x10000000;
constexpr uintptr_t kRender2DAddr = 0x100AE370;  // C2DPrimitiveManager::Render (preferred base)
typedef int(__fastcall *Render2DFn)(void *self, int edx, int a1, int a2);
Render2DFn g_orig_render2d = nullptr;

// C2DPrimitiveManager::Render detour: draw the plates just before the UI raster, once per scene.
int __fastcall Render2D_hk(void *self, int edx, int a1, int a2) {
  // Skip our pre-UI draw during teardown; chain straight to the client's raster.
  if (crash_handler::shutting_down()) return g_orig_render2d(self, edx, a1, a2);
  if (g_np_draw_pending) {
    g_np_draw_pending = false;
    void *pRender = *kRenderInterface;
    IDirect3DDevice9 *dev =
        pRender ? *reinterpret_cast<IDirect3DDevice9 **>(static_cast<char *>(pRender) + kRenderDeviceOffset)
                : nullptr;
    if (dev) {
      rcp_guard::run("font.nameplates", [&] { on_render_nameplates(dev); });
      // Extra world-space overlays (target ring) share this post-world/pre-UI seam.
      if (g_scene_draws)
        for (auto &cb : *g_scene_draws) rcp_guard::run("font.scene_draw", [&] { cb(dev); });
    }
  }
  return g_orig_render2d(self, edx, a1, a2);
}

// Registers the frame-marker callback + the pre-UI-raster draw detour once, lazily, as soon as the
// render interface exists. Driven from the EndScene callback (per frame) until it takes. Safe to
// patch here: C2DPrimitiveManager::Render is never on the stack while device EndScene runs.
void ensure_render_callback(IDirect3DDevice9 * /*unused*/) {
  if (g_render_cb_registered) return;
  void *pRender = *kRenderInterface;
  if (!pRender) return;
  void **vtbl = *reinterpret_cast<void ***>(pRender);
  if (!vtbl) return;
  auto rcp = RcpService::get_instance();
  if (!rcp || !rcp->hooks) return;
  HMODULE gfx = GetModuleHandleA("EQGraphicsDX9.dll");
  if (!gfx) return;
  auto set_cb = reinterpret_cast<SetRenderCallbackFn>(vtbl[kSetRenderCallbackVtblIndex]);
  g_prev_render_cb = set_cb(pRender, &scene_render_cb);
  const uintptr_t render2d = reinterpret_cast<uintptr_t>(gfx) + (kRender2DAddr - kGfxPreferredBase);
  rcp->hooks->Add("font_render2d", static_cast<int>(render2d), Render2D_hk, hook_type_detour);
  g_orig_render2d = rcp->hooks->hook_map["font_render2d"]->original(Render2D_hk);
  g_render_cb_registered = true;
  logger::logf("[font] nameplate seam installed: frame-marker cb + pre-UI raster detour @%08X",
               static_cast<unsigned>(render2d));
}

}  // namespace

bool font_overlay::get_enabled() { return g_np_enabled; }

void font_overlay::set_enabled(bool enabled) {
  g_np_enabled = enabled;
  if (enabled) g_stat_log = 8;
  apply_and_save();
}

bool font_overlay::get_show_hp() { return g_show_hp; }
bool font_overlay::get_show_mana() { return g_show_mana; }
bool font_overlay::get_show_stam() { return g_show_stam; }

void font_overlay::set_bars(bool hp, bool mana, bool stam) {
  g_show_hp = hp;
  g_show_mana = mana;
  g_show_stam = stam;
  save_settings();
}

float font_overlay::get_max_dist() { return g_np_max_dist; }
float font_overlay::max_dist_cap() { return kNpMaxDistCap; }

void font_overlay::set_max_dist(float dist) {
  g_np_max_dist = dist;
  save_settings();
}

void font_overlay::add_scene_draw(std::function<void(IDirect3DDevice9 *)> cb) {
  if (!g_scene_draws) g_scene_draws = new std::vector<std::function<void(IDirect3DDevice9 *)>>();
  g_scene_draws->push_back(std::move(cb));
}

FontOverlay::FontOverlay(RcpService *rcp) {
  // Restore persisted settings, then apply the native-suppression state to match.
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "Billboard")) g_np_enabled = ini.getValue<bool>(kIniSection, "Billboard");
  if (ini.exists(kIniSection, "Offset")) g_np_offset_lines = ini.getValue<float>(kIniSection, "Offset");
  if (ini.exists(kIniSection, "Scale")) g_np_scale = ini.getValue<float>(kIniSection, "Scale");
  if (ini.exists(kIniSection, "MaxDist")) g_np_max_dist = ini.getValue<float>(kIniSection, "MaxDist");
  if (ini.exists(kIniSection, "HpBar")) g_show_hp = ini.getValue<bool>(kIniSection, "HpBar");
  if (ini.exists(kIniSection, "ManaBar")) g_show_mana = ini.getValue<bool>(kIniSection, "ManaBar");
  if (ini.exists(kIniSection, "StamBar")) g_show_stam = ini.getValue<bool>(kIniSection, "StamBar");
  nameplate::set_suppress_native(g_np_enabled);

  directx::add_render_callback(on_render);
  directx::add_render_callback(on_render_3d);
  // Nameplates draw from the client's in-scene render callback (post-world, pre-UI), NOT from
  // EndScene (post-UI, which drew them over the UI). The EndScene callback below just registers it
  // once the render interface is up.
  directx::add_render_callback(ensure_render_callback);

  rcp->commands_hook->Add(
      "/rcpfont", {},
      "Custom-font 3D billboard nameplates. '/rcpfont on|off', '/rcpfont offset <lines>' (height "
      "above head), '/rcpfont scale <n>' (text size, default 0.04). Probes: 'test', 'test3d'.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2 && (args[1] == "on" || args[1] == "off" || args[1] == "1" || args[1] == "0")) {
          g_np_enabled = (args[1] == "on" || args[1] == "1");
          if (g_np_enabled) g_stat_log = 8;  // Log the raw self-stat reads for a few frames.
          apply_and_save();
          Rcp::Game::print_chat("rof2ClientPlus: billboard nameplates %s", g_np_enabled ? "ON" : "OFF");
        } else if (args.size() >= 3 && (args[1] == "offset" || args[1] == "o")) {
          try {
            g_np_offset_lines = g_test3d_offset_lines = std::stof(args[2]);
            save_settings();
            Rcp::Game::print_chat("rof2ClientPlus: nameplate offset = %.2f lines", g_np_offset_lines);
          } catch (...) {
            Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpfont offset <lines>' (e.g. 1.0)");
          }
        } else if (args.size() >= 3 && (args[1] == "scale" || args[1] == "s")) {
          try {
            g_np_scale = std::stof(args[2]);
            save_settings();
            Rcp::Game::print_chat("rof2ClientPlus: nameplate scale = %.4f", g_np_scale);
          } catch (...) {
            Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpfont scale <n>' (e.g. 0.04)");
          }
        } else if (args.size() >= 3 && (args[1] == "dist" || args[1] == "distance")) {
          try {
            g_np_max_dist = std::stof(args[2]);
            save_settings();
            Rcp::Game::print_chat("rof2ClientPlus: nameplate max distance = %.0f", g_np_max_dist);
          } catch (...) {
            Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpfont dist <units>' (e.g. 1000)");
          }
        } else if (args.size() >= 3 && (args[1] == "hp" || args[1] == "mana" || args[1] == "stam")) {
          const bool on = (args[2] == "on" || args[2] == "1");
          font_overlay::set_bars(args[1] == "hp" ? on : g_show_hp, args[1] == "mana" ? on : g_show_mana,
                                 args[1] == "stam" ? on : g_show_stam);
          Rcp::Game::print_chat("rof2ClientPlus: %s bar %s", args[1].c_str(), on ? "ON" : "OFF");
        } else if (args.size() >= 2 && (args[1] == "test3d" || args[1] == "3d")) {
          if (args.size() >= 3)
            g_test3d_enabled = (args[2] == "on" || args[2] == "1");
          else
            g_test3d_enabled = !g_test3d_enabled;
          Rcp::Game::print_chat("rof2ClientPlus: font test3d %s", g_test3d_enabled ? "ON" : "OFF");
        } else if (args.size() >= 2 && (args[1] == "test" || args[1] == "t")) {
          if (args.size() >= 3)
            g_test_enabled = (args[2] == "on" || args[2] == "1");
          else
            g_test_enabled = !g_test_enabled;  // '/rcpfont test' toggles.
          Rcp::Game::print_chat("rof2ClientPlus: font test %s", g_test_enabled ? "ON" : "OFF");
        } else {
          Rcp::Game::print_chat("rof2ClientPlus: usage '/rcpfont test on|off | test3d on|off'");
        }
        return true;
      });

  logger::log("[font] FontOverlay constructed; /rcpfont registered");
}

FontOverlay::~FontOverlay() { g_test_font.reset(); }
