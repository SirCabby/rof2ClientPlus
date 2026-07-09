// rof2ClientPlus - custom-font overlay (nameplate font/bars overhaul, "N4").
//
// Owns the bitmap-font engine and draws into each frame via a directx render
// callback (EndScene). '/rcpfont test|test3d' render probes; '/rcpfont on' draws the
// real 3-D billboard nameplates (name + health/mana/stamina bars) for every visible
// PC/NPC by walking the client's spawn list.
#pragma once

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
}  // namespace font_overlay
