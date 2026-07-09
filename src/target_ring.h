// rof2ClientPlus - target ring (Zeal target_ring port, RoF2): solid color or an optional graphic.
//
// Draws a flat, solid-color "donut" ring on the ground centered under the current target: a filled
// band between an outer radius and a smaller inner radius (the donut hole). Self-contained with raw
// stock-RoF2 addresses (chase_cam / no_fog style) - the vendored game_functions/addresses still
// hold TAKP offsets and are NOT used here.
//
// The ring renders at the shared post-world / pre-UI in-scene seam (font_overlay::add_scene_draw),
// so it z-tests against terrain and walls (occluded by geometry in front) and is painted over by
// UI windows - exactly like the native target indicator. Fixed-function XYZ|DIFFUSE triangle strip,
// no texture, drawn with DrawPrimitiveUP (no vertex-buffer lifecycle / device-lost handling needed).
//
// The remaining Zeal extras (spin, 3-D cone, auto-attack blink, match-heading) are intentionally
// dropped, but an optional ring GRAPHIC is supported like Zeal: drop a .tga into
// uifiles/rcp/targetrings and select it, and it maps onto the donut band (u = inner..outer,
// v = around the circle) modulated by the ring color/opacity - Zeal's own ring textures work
// unchanged. '/rcpring [on|off|outer N|inner N|opacity 0-1|color RRGGBB|con on|off|self on|off|
// graphic <name>|none]'. Persisted to rof2ClientPlus.ini [TargetRing]. Off by default so nothing
// changes until the user opts in.
#pragma once

#include <string>
#include <vector>

class RcpService;

class TargetRing {
 public:
  explicit TargetRing(RcpService *rcp);

 private:
  RcpService *rcp_ = nullptr;
};

// Accessors for the options-window UI to read/write the live ring settings. Every setter applies
// immediately (the next drawn frame) and persists to rof2ClientPlus.ini [TargetRing].
namespace target_ring_settings {
bool get_enabled();
void set_enabled(bool on);
int get_color();  // 0xRRGGBB (no alpha - opacity is separate). Used when con coloring is off.
void set_color(int rgb);
bool get_use_con_color();  // true = color the ring by the target's con level instead of the fixed color.
void set_use_con_color(bool on);
float get_outer();  // Outer radius, world units.
void set_outer(float r);
float get_inner();  // Inner radius (donut hole), world units; clamped to <= outer at draw time.
void set_inner(float r);
float get_opacity();  // 0..1 (1 = fully solid).
void set_opacity(float a);
bool get_hide_self();  // Skip drawing when the target is yourself.
void set_hide_self(bool on);
float radius_max();  // Upper bound the sliders/commands allow for both radii.
std::string get_graphic();                  // Current ring graphic name, or "" for the solid ring.
void set_graphic(const std::string &name);  // "" or "none"/"None" selects the solid ring (no texture).
std::vector<std::string> get_available_graphics();  // "None", then each *.tga stem in uifiles/rcp/targetrings.
bool get_spin();       // true = the ring graphic slowly rotates; false = it faces the target's heading.
void set_spin(bool on);
}  // namespace target_ring_settings
