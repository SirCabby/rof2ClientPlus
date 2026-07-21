// rof2ClientPlus - live classic/revamped spell-icon swap.
//
// The Feb-2013 icon revamp replaced the ART inside the same ten sheet files the
// UI has used since 2001: Spells01-07.tga (the 40x40-cell A_SpellIcons grid:
// spell book, buff windows, casting bar, item effects) and gemicons01-03.tga
// (the 24x24-cell A_SpellGems grid: the spell-gem fills). Every widget that
// shows a spell icon resolves its frame's texture through the ONE global
// CEQSuiteTextureLoader entry for that sheet, down to a CEQGBitmap whose
// m_pD3DTexture holds the pixels. Swapping that one D3D texture pointer per
// sheet therefore retargets every icon user at once - live, no file swap, no
// /loadskin, no relog. The classic art ships as uifiles/rcp/spellicons/*.tga
// and is loaded with D3DX on the render thread the first time it's needed.
//
// '/rcpspellicons on|off' (alias /rcpicons) toggles; a "Classic spell icons"
// checkbox lives on the Display tab of /rcpoptions. Persists in
// rof2ClientPlus.ini [SpellIcons].
#pragma once

class RcpService;

class SpellIcons {
 public:
  explicit SpellIcons(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for the options-window UI to read/write the live setting.
namespace spell_icons_settings {
bool get_classic();         // true = the original pre-2013 icon art is shown.
void set_classic(bool on);  // Applies on the next frame + persists to ini.
}  // namespace spell_icons_settings

// Shared UI helpers other modules reuse (spellbook_ui's icon cells).
namespace spell_icons_api {
// Find a Ui2DAnimation (CTextureAnimation*) by name with a READ-ONLY walk of the
// SIDL manager's animation array - no CXStr temp, safe from any thread. Returns
// the game-owned animation object (do not free), or null before the UI loads.
char *find_animation(const char *name);
}  // namespace spell_icons_api
