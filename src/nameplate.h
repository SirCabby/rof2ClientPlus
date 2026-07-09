// rof2ClientPlus - character nameplate tinting + text mods (Zeal nameplate port, N1+N2), RoF2.
//
// Recolors and reformats the floating name text above entities. Self-contained with raw
// stock-RoF2 addresses (chase_cam.cpp / mouse_mods.cpp style): the vendored game_functions /
// game_addresses / game_structures still hold TAKP offsets and are NOT used here.
//
// Mechanism:
//  - Tint (N1): detour PlayerClient::SetNameSpriteTint (0x58BF00, __thiscall, this == the
//    entity). When a coloring option is enabled we compute our own color and apply it via
//    the entity's actor-interface vtable (slot +0x190) - the same final step the client does.
//  - Text (N2): detour PlayerClient::SetNameSpriteString (0x58BE90, __thiscall) - the client's
//    sole path for pushing the built nameplate text to the sprite - and transform that text
//    (target markers, target health %, hide self).
// With every option off both detours fall straight through to the originals, so installing
// them at load time changes nothing until the user opts in. See PORTING_NOTES.md (Nameplate).
#pragma once

#include <string>

class RcpService;

class NamePlate {
 public:
  NamePlate(RcpService *rcp);

 private:
  RcpService *rcp_ = nullptr;
};

// Accessors for the options-window UI (Phase N6) to read/write the live settings.
// (Player-name generation is always active - not a setting.)
namespace nameplate_settings {
bool get_con_colors();     // Tint NPCs by con level.
bool get_state_colors();   // Tint players by guild/AFK/LFG/LD/roleplay/PVP; corpses too.
bool get_target_color();   // Highlight the current target.
bool get_target_blink();   // Pulse the target's nameplate (any coloring mode, PCs + NPCs).
bool get_target_marker();  // Wrap the target's name in >> <<.
bool get_target_health();  // Append the target's HP percent.
bool get_hide_self();      // Blank your own nameplate (unless it's the target).
int get_blink_ms();        // Target-blink full-cycle period (200-3000 ms).
void set_blink_ms(int ms); // Sets + persists the blink period.
void set(bool con_colors, bool state_colors, bool target_color, bool target_blink, bool target_marker,
         bool target_health, bool hide_self);  // Applies live + persists.
}  // namespace nameplate_settings

// Editable nameplate color palette for the options-window color picker (N6). Each "role"
// (con levels, player states, target, corpse) maps to an 0xRRGGBB color.
namespace nameplate_colors {
int count();                  // Number of editable color roles.
const char *name(int role);   // Human label for a role (shown in the role selector).
int get(int role);            // Current 0xRRGGBB color of a role.
void set(int role, int rgb);  // Set + persist a role's color (applies live).
}  // namespace nameplate_colors

// Helpers the custom-font billboard nameplates (font_overlay, N4c) reuse so the billboards match
// native content: the display text (name line per /shownames level - title/first/last/guild/AFK)
// and the con/state color. These read the same entity fields the native nameplate uses.
namespace nameplate {
std::string billboard_text(void *entity);  // Name line; empty if the entity has no drawable name.
int billboard_color(void *entity);         // 0xRRGGBB, con/state colored; client-like default otherwise.
// When true, the native name text is blanked for every entity (via the set-string hook, which keeps
// the con-tint firing) so the custom billboard nameplates don't double up with the client's own.
void set_suppress_native(bool suppress);
}  // namespace nameplate
