// rof2ClientPlus - raise the world/terrain and actor view distance. See view_distance.h.
#include "view_distance.h"
#include "rebase.h"

#include <windows.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "commands.h"
#include "directx.h"
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// --- Reverse-engineered from eqgame.exe (May-2013 RoF2 build). See the
// view-distance-re memory / PORTING_NOTES for the full derivation. ---
//
// *(void**)0xDD2660 is the render camera/view object. Three distance fields on it
// are fed to the graphics-engine camera (the sub-object at camera+0x118) when the
// projection is rebuilt by 0x48f290:
//   camera+0x0008 (float) -> vtbl +0x0c : world/terrain FAR clip (raw world units)
//   camera+0x2d78 (int)   -> vtbl +0x14 : actor clip; engine distance = int*10 + 50
//   camera+0x2d7c (int)   -> vtbl +0x1c : SHADOW clip  (WE NEVER TOUCH THIS)
// The fields are only written on discrete events (slider move, settings apply,
// zone load), never per frame, so poking the source ini ints does nothing. We set
// the fields directly and call the projection rebuild, and re-assert on drift
// (a fresh zone resets them to stock).
const uintptr_t kCameraPtr = ::Rcp::eqva(0x00DD2660);      // -> camera/view object (may be null off-scene).
constexpr uintptr_t kCamGfxOffset = 0x118;        // -> graphics-engine camera sub-object (null off-scene).
constexpr uintptr_t kFarClipOffset = 0x8;         // float: terrain far clip (raw world units).
constexpr uintptr_t kActorClipOffset = 0x2d78;    // int: actor clip; world units = int*10 + 50.
const uintptr_t kRebuildProjection = ::Rcp::eqva(0x0048F290);  // void __thiscall(camera): rebuild + re-push all clips.

// Rebuild the projection so field changes reach the graphics engine.
inline void rebuild_projection(void *camera) {
  reinterpret_cast<void(__thiscall *)(void *)>(kRebuildProjection)(camera);
}

// Actor clip is stored as int with engine distance = int*10 + 50; convert to/from
// world units so the command speaks the same units as the terrain far clip.
inline int actor_units_to_int(int units) {
  int v = (units - 50) / 10;
  return v < 0 ? 0 : v;
}
inline int actor_int_to_units(int i) { return i * 10 + 50; }

constexpr char kIniSection[] = "ViewDistance";
constexpr char kFarKey[] = "FarClip";
constexpr char kActorKey[] = "ActorClip";

// 0 == that layer is off (leave the client's own value alone). Otherwise the
// target distance in world units.
int g_far = 0;
int g_actor = 0;

// Stock values captured the first time we override each field, for restore-on-off.
bool g_far_captured = false;
float g_far_saved = 0.0f;
bool g_actor_captured = false;
int g_actor_saved = 0;

// Returns the live camera, or nullptr off-scene (char select / loading). Also
// validates the graphics sub-object the rebuild dereferences without a null
// check, so calling into the client is safe.
void *live_camera() {
  void *camera = *reinterpret_cast<void **>(kCameraPtr);
  if (!camera) return nullptr;
  void *gfx = *reinterpret_cast<void **>(reinterpret_cast<char *>(camera) + kCamGfxOffset);
  if (!gfx) return nullptr;
  return camera;
}
inline float *far_field(void *c) { return reinterpret_cast<float *>(reinterpret_cast<char *>(c) + kFarClipOffset); }
inline int *actor_field(void *c) { return reinterpret_cast<int *>(reinterpret_cast<char *>(c) + kActorClipOffset); }

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kFarKey)) g_far = ini.getValue<int>(kIniSection, kFarKey);
  if (ini.exists(kIniSection, kActorKey)) g_actor = ini.getValue<int>(kIniSection, kActorKey);
  if (g_far < 0) g_far = 0;
  if (g_actor < 0) g_actor = 0;
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<int>(kIniSection, kFarKey, g_far);
  ini.setValue<int>(kIniSection, kActorKey, g_actor);
}

// Push whichever layers are enabled onto the camera and rebuild once. Snapshots
// the stock value the first time each field is overridden.
void apply(void *camera) {
  if (g_far > 0) {
    if (!g_far_captured) {
      g_far_captured = true;
      g_far_saved = *far_field(camera);
    }
    *far_field(camera) = static_cast<float>(g_far);
  }
  if (g_actor > 0) {
    if (!g_actor_captured) {
      g_actor_captured = true;
      g_actor_saved = *actor_field(camera);
    }
    *actor_field(camera) = actor_units_to_int(g_actor);
  }
  rebuild_projection(camera);
}

// Restore any layer whose target is off, if we had captured a stock value. Called
// with a live camera. Returns true if something was restored (needs a rebuild).
bool restore_disabled(void *camera) {
  bool changed = false;
  if (g_far <= 0 && g_far_captured) {
    *far_field(camera) = g_far_saved;
    g_far_captured = false;
    changed = true;
  }
  if (g_actor <= 0 && g_actor_captured) {
    *actor_field(camera) = g_actor_saved;
    g_actor_captured = false;
    changed = true;
  }
  return changed;
}

// Runs on every EndScene. Re-assert enabled layers if the live values have
// drifted (a fresh zone resets them). Common case: a couple of compares, no call.
void on_render(IDirect3DDevice9 * /*dev*/) {
  if (g_far <= 0 && g_actor <= 0) return;
  void *camera = live_camera();
  if (!camera) return;
  bool drift = false;
  if (g_far > 0 && std::fabs(*far_field(camera) - static_cast<float>(g_far)) > 0.5f) drift = true;
  if (g_actor > 0 && *actor_field(camera) != actor_units_to_int(g_actor)) drift = true;
  if (drift) apply(camera);
}

void print_status() {
  void *camera = live_camera();
  Rcp::Game::print_chat("rof2ClientPlus view distance:");
  if (camera) {
    Rcp::Game::print_chat("  terrain: %s (live far clip = %.0f)",
                          g_far > 0 ? std::to_string(g_far).append(" units").c_str() : "OFF", *far_field(camera));
    Rcp::Game::print_chat("  actors:  %s (live = %d units)",
                          g_actor > 0 ? std::to_string(g_actor).append(" units").c_str() : "OFF",
                          actor_int_to_units(*actor_field(camera)));
  } else {
    Rcp::Game::print_chat("  terrain: %s | actors: %s  (applies once you're in a zone)",
                          g_far > 0 ? std::to_string(g_far).append(" units").c_str() : "OFF",
                          g_actor > 0 ? std::to_string(g_actor).append(" units").c_str() : "OFF");
  }
}

// Shared setter used by both commands and the (future) options UI.
void set_layer(int *layer, int units) {
  if (units < 0) units = 0;
  *layer = units;
  save_settings();
  void *camera = live_camera();
  if (!camera) return;  // on_render applies once we're in a scene.
  bool restored = restore_disabled(camera);
  if (g_far > 0 || g_actor > 0)
    apply(camera);  // (re)apply enabled layers; also rebuilds.
  else if (restored)
    rebuild_projection(camera);  // only a restore happened; push it.
}

// Parse "<n>" / "off" for a command; returns false if the arg was invalid (message printed).
bool parse_and_set(std::vector<std::string> &args, int *layer, const char *what) {
  if (args.size() >= 2) {
    const std::string &a = args[1];
    if (a == "off" || a == "0" || a == "restore") {
      set_layer(layer, 0);
    } else {
      int n = std::atoi(a.c_str());
      if (n < 1) {
        Rcp::Game::print_chat("rof2ClientPlus: '%s' is not a valid %s distance (use n>=1 world units, or 'off').",
                              a.c_str(), what);
        return false;
      }
      set_layer(layer, n);
    }
  }
  return true;
}

}  // namespace

namespace view_distance_settings {
int get_clip() { return g_far; }
void set_clip(int clip) { set_layer(&g_far, clip); }
int get_actor_clip() { return g_actor; }
void set_actor_clip(int clip) { set_layer(&g_actor, clip); }
}  // namespace view_distance_settings

ViewDistance::ViewDistance(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  logger::logf("[viewdist] settings loaded: far=%d actor=%d (applies on first world frame)", g_far, g_actor);

  directx::add_render_callback(on_render);

  rcp->commands_hook->Add(
      "/rcpviewdist", {"/rcpview", "/rcpclip"},
      "Terrain view distance: '/rcpviewdist <n>' forces the far clip to n world units, "
      "'/rcpviewdist off' restores the default, '/rcpviewdist' prints status.",
      [](std::vector<std::string> &args) {
        if (parse_and_set(args, &g_far, "terrain")) print_status();
        return true;
      });

  rcp->commands_hook->Add(
      "/rcpactordist", {"/rcpactorclip", "/rcpmobdist"},
      "Actor (NPC/player) view distance: '/rcpactordist <n>' draws actors out to n world units, "
      "'/rcpactordist off' restores the default. Does not affect shadows.",
      [](std::vector<std::string> &args) {
        if (parse_and_set(args, &g_actor, "actor")) print_status();
        return true;
      });
  logger::log("[viewdist] /rcpviewdist + /rcpactordist registered");
}
