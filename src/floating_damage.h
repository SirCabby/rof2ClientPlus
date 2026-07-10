// rof2ClientPlus - floating combat damage (Zeal floating_damage port, RoF2).
//
// Draws rising, fading damage numbers over each entity as it is struck - the classic Zeal
// "floating combat text" feature. The damage events come from a detour on the client's own
// CEverQuest::ReportSuccessfulHit (0x52EE40; struct EQSuccessfulHit from eqlib, which is
// compiled for our exact 2013-05-10 build), the same function Zeal hooks on TAKP. Each number
// is drawn as a 3-D billboard (SpriteFont) at the struck entity's head anchor - reusing the
// in-game-confirmed nameplate render path (font_overlay::add_scene_draw + the shared arial_bold_24
// atlas) - so it z-tests against terrain/walls and is painted over by UI windows, exactly like
// the nameplates. Numbers rise in world-Z and fade out over ~2 seconds; big hits get a distinct
// color, a larger size, and a longer life.
//
// '/rcpfcd' toggles; subcommands set the per-source filters, the big-hit threshold, and the
// colors. Also exposed on the /rcpoptions "Combat" tab. Persisted to rof2ClientPlus.ini
// [FloatingDamage]. Off by default so nothing changes until the user opts in.
#pragma once

class RcpService;

class FloatingDamage {
 public:
  explicit FloatingDamage(RcpService *rcp);

 private:
  RcpService *rcp_ = nullptr;
};

// Accessors for the options-window UI (and the /rcpfcd command) to read/write the live settings.
// Every setter applies immediately (the next struck entity / drawn frame) and persists to
// rof2ClientPlus.ini [FloatingDamage]. Colors are 0xRRGGBB (opacity is the animated fade).
namespace floating_damage_settings {
bool get_enabled();
void set_enabled(bool on);
bool get_show_mine();  // Damage dealt by me or my pet.
void set_show_mine(bool on);
bool get_show_incoming();  // Damage dealt to me.
void set_show_incoming(bool on);
bool get_show_others();  // Everyone else's damage (mob-on-mob, other players/pets).
void set_show_others(bool on);
bool get_show_melee();  // Melee (non-spell) hits.
void set_show_melee(bool on);
bool get_show_spells();  // Spell / non-melee hits (spell id > 0).
void set_show_spells(bool on);
int get_big_hit();  // Damage >= this uses the big-hit color/size/life.
void set_big_hit(int threshold);
int big_hit_cap();  // Upper bound the slider/command allow for the threshold.
int get_color_mine();
void set_color_mine(int rgb);
int get_color_incoming();
void set_color_incoming(int rgb);
int get_color_other();
void set_color_other(int rgb);
int get_color_crit();  // Big-hit color.
void set_color_crit(int rgb);
}  // namespace floating_damage_settings
