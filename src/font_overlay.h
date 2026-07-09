// rof2ClientPlus - custom-font overlay (nameplate font/bars overhaul, "N4").
//
// Owns the bitmap-font engine and draws into each frame via a directx render
// callback (EndScene). '/rcpfont test|test3d' render probes; '/rcpfont on' draws the
// real 3-D billboard nameplates (name + health/mana/stamina bars) for every visible
// PC/NPC by walking the client's spawn list.
#pragma once

#include <functional>

struct IDirect3DDevice9;

class FontOverlay {
 public:
  FontOverlay(class RcpService *rcp);
  ~FontOverlay();
};

// Accessors for the options-window UI (and any caller) to read/toggle the billboard-nameplate
// feature. set_enabled applies native-name suppression + persists, exactly like '/rcpfont on|off'.
namespace font_overlay {
bool get_enabled();
void set_enabled(bool enabled);
// Per-bar toggles (HP for all entities; mana/stamina self only). set_bars applies live + persists.
bool get_show_hp();
bool get_show_mana();
bool get_show_stam();
void set_bars(bool hp, bool mana, bool stam);
// Max draw distance (world units). set_max_dist persists; max_dist_cap is the slider's top value.
float get_max_dist();
float max_dist_cap();
void set_max_dist(float dist);

// Register an extra draw at the shared post-world / pre-UI in-scene seam - the same
// C2DPrimitiveManager::Render detour the billboard nameplates use. Callbacks run on the render
// thread with the live device, already rcp_guard-wrapped, AFTER the world raster (so geometry
// z-tests against terrain/walls) and BEFORE the UI raster (so windows occlude it). This is the
// correct seam for world-space overlays like the target ring; it installs whenever FontOverlay
// exists, independent of whether billboard nameplates are enabled.
void add_scene_draw(std::function<void(IDirect3DDevice9 *)> cb);
}  // namespace font_overlay
