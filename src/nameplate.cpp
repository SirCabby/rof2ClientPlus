// rof2ClientPlus - character nameplate tinting + text mods (Zeal nameplate port, N1+N2), RoF2.
// See nameplate.h and PORTING_NOTES.md (Nameplate) for the mechanism + full RE.
#include "nameplate.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

// ---- Stock RoF2 addresses (eqlib-sourced, disasm-confirmed; see PORTING_NOTES) ----
// PlayerClient::SetNameSpriteTint(), __thiscall(this == entity), no args, returns int.
static constexpr uintptr_t kSetNameSpriteTint = 0x58BF00;
// PlayerClient::SetNameSpriteState(this, bool show), __thiscall, ret 0x4. Rebuilds the whole
// nameplate text (-> SetNameSpriteString -> SetNameSpriteTint). The client only calls this on
// its own slow timer / on some state changes, NOT on targeting or HP change - so we call it
// ourselves to make text mods take effect immediately when the target/self/HP changes.
static constexpr uintptr_t kSetNameSpriteState = 0x58E2D0;
static void **const kSelf = reinterpret_cast<void **>(0xDD2630);    // local player
static void **const kTarget = reinterpret_cast<void **>(0xDD2648);  // current target

// PlayerClient (Entity) field offsets.
static constexpr int kEntActor = 0x101c;   // CActorInterface* (all-virtual); apply tint via its vtable
static constexpr int kEntType = 0x125;     // uint8: 0=Player, 1=NPC, 2=Corpse
static constexpr int kEntLevel = 0x250;    // uint8
static constexpr int kEntAnon = 0x2b8;     // int: 1=anonymous, 2=roleplay
static constexpr int kEntHPMax = 0x2dc;    // int32
static constexpr int kEntHPCurrent = 0x2e4; // int32 (for NPCs the client only knows a percent)
static constexpr int kEntPvP = 0x349;      // uint8 (bool) PvPFlag
static constexpr int kEntGuildID = 0x34c;  // int32, -1 = unguilded
static constexpr int kEntAFK = 0x3c0;      // int
static constexpr int kEntLinkdead = 0x3d0; // uint8 (bool)
static constexpr int kEntLFG = 0x440;      // uint8 (bool)
static constexpr int kEntDisplayNameSprite = 0x1228;  // uint8 (bool); SetNameSpriteState early-outs if 0

// Entity Type enum (RoF2: SPAWN_PLAYER/NPC/CORPSE from eqlib Constants.h).
static constexpr uint8_t kTypePlayer = 0;
static constexpr uint8_t kTypeNPC = 1;
static constexpr uint8_t kTypeCorpse = 2;

// The tint is applied by the entity's actor interface: actor->vtable[+0x190](colorPtr),
// where colorPtr points at 3 color bytes the client builds on its stack (index = 0x190/4).
static constexpr int kActorSetTintVtableIndex = 0x190 / 4;
// actor->vtable[+0x1a4](0) returns a bool: is the name sprite currently shown. We use it so a
// forced rebuild honors the client's own visibility (never conjures a nameplate that's off).
static constexpr int kActorNameShownVtableIndex = 0x1a4 / 4;
// actor->vtable[+0x18c](flag, char* text, char* scratch) sets the name-sprite string. This is
// the UNIVERSAL text seam: SetNameSpriteState calls it inline in the normal path (0x58efe7) and
// SetNameSpriteString (0x58be90) wraps it for the "extended nameplate" path. We detour this
// function (its address read from a live actor) so our text transform runs in every mode - the
// 0x58be90 wrapper is only used in extended mode, which stock RoF2 isn't in by default.
static constexpr int kActorSetStringVtableIndex = 0x18c / 4;

// ---- Default colors (0xRRGGBB). Hardcoded for N1; wired to the options window in N6.
// The con colors mirror the get_con_* defaults already used in game_functions.cpp. ----
static constexpr int kColWhite = 0xf0f0f0;
static constexpr int kColRed = 0xf00000;
static constexpr int kColBlue = 0x0040f0;
static constexpr int kColYellow = 0xf0f000;
static constexpr int kColLightBlue = 0x00f0f0;
static constexpr int kColGreen = 0x00f000;

static constexpr int kColAFK = 0x808080;
static constexpr int kColLFG = 0xf0f000;
static constexpr int kColLD = 0x606060;
static constexpr int kColRole = 0xf000f0;
static constexpr int kColPVP = 0xf00000;
static constexpr int kColMyGuild = 0x00f000;
static constexpr int kColOtherGuild = 0xf0f0f0;
static constexpr int kColAdventurer = 0x00f0f0;
static constexpr int kColCorpse = 0x909090;
static constexpr int kColTarget = 0xff8000;  // Orange: deliberately asymmetric to verify byte order.

// ---- Live settings (persisted to rof2ClientPlus.ini [Nameplate]). Off by default so
// nothing changes until the user opts in, matching chase_cam / mouse_mods. ----
// Tint (N1).
static bool g_con_colors = false;
static bool g_state_colors = false;
static bool g_target_color = false;
static bool g_target_blink = false;
// Text (N2).
static bool g_target_marker = false;  // Wrap the target's name in >> <<.
static bool g_target_health = false;  // Append the target's HP percent.
static bool g_hide_self = false;      // Blank your own nameplate (unless it's the target).

// Forced text-rebuild bookkeeping (N2). The client rebuilds nameplate text lazily, so we
// re-drive SetNameSpriteState ourselves when the thing we modify changes.
static bool g_setstr_installed = false;              // Whether the actor set-string detour is installed yet.
static bool g_refreshing = false;                    // Recursion guard (our forced rebuild re-enters the tint hook).
static void *g_last_target = reinterpret_cast<void *>(-1);  // Target we last refreshed (-1 = force on first sight).
static void *g_prev_target = nullptr;                // Prior target awaiting a cleanup rebuild (strip its marker).
static int g_last_target_hp = -1;                    // Target HP% we last rendered (for targethealth).
static bool g_self_dirty = false;                    // Self nameplate needs one rebuild (toggle of hideself).
static int g_refresh_log = 0;                        // Small budget: log the first forced rebuilds.
static int g_text_log = 0;                           // Small budget: log the first text transforms.
static int g_diag = 0;                               // Diagnostic budget: dump entity/target/self pointers.

// Marks text state dirty so the next frame re-pushes the target + self nameplates. Called on
// any text-option toggle so the change is visible without waiting for the client's own timer.
static void mark_text_dirty() {
  g_last_target = reinterpret_cast<void *>(-1);
  g_last_target_hp = -1;
  g_self_dirty = true;
  g_refresh_log = 12;
  g_text_log = 40;  // transform (xf) log budget
  g_diag = 150;     // refresh (mrt) log budget
}

static constexpr char kIniSection[] = "Nameplate";

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "ConColors")) g_con_colors = ini.getValue<bool>(kIniSection, "ConColors");
  if (ini.exists(kIniSection, "StateColors")) g_state_colors = ini.getValue<bool>(kIniSection, "StateColors");
  if (ini.exists(kIniSection, "TargetColor")) g_target_color = ini.getValue<bool>(kIniSection, "TargetColor");
  if (ini.exists(kIniSection, "TargetBlink")) g_target_blink = ini.getValue<bool>(kIniSection, "TargetBlink");
  if (ini.exists(kIniSection, "TargetMarker")) g_target_marker = ini.getValue<bool>(kIniSection, "TargetMarker");
  if (ini.exists(kIniSection, "TargetHealth")) g_target_health = ini.getValue<bool>(kIniSection, "TargetHealth");
  if (ini.exists(kIniSection, "HideSelf")) g_hide_self = ini.getValue<bool>(kIniSection, "HideSelf");
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "ConColors", g_con_colors);
  ini.setValue<bool>(kIniSection, "StateColors", g_state_colors);
  ini.setValue<bool>(kIniSection, "TargetColor", g_target_color);
  ini.setValue<bool>(kIniSection, "TargetBlink", g_target_blink);
  ini.setValue<bool>(kIniSection, "TargetMarker", g_target_marker);
  ini.setValue<bool>(kIniSection, "TargetHealth", g_target_health);
  ini.setValue<bool>(kIniSection, "HideSelf", g_hide_self);
}

namespace nameplate_settings {
bool get_con_colors() { return g_con_colors; }
bool get_state_colors() { return g_state_colors; }
bool get_target_color() { return g_target_color; }
bool get_target_blink() { return g_target_blink; }
bool get_target_marker() { return g_target_marker; }
bool get_target_health() { return g_target_health; }
bool get_hide_self() { return g_hide_self; }
void set(bool con_colors, bool state_colors, bool target_color, bool target_blink, bool target_marker,
         bool target_health, bool hide_self) {
  g_con_colors = con_colors;
  g_state_colors = state_colors;
  g_target_color = target_color;
  g_target_blink = target_blink;
  g_target_marker = target_marker;
  g_target_health = target_health;
  g_hide_self = hide_self;
  save_settings();
}
}  // namespace nameplate_settings

// ---- Con color: reproduces the level-band table from Rcp::Game::GetLevelCon locally so
// this module stays self-contained (that function reaches through get_user_color / the
// unconstructed options UI, which is unsafe in this build). Returns an 0xRRGGBB color. ----
static int con_color(int my_level, int ent_level) {
  const int diff = ent_level - my_level;
  if (diff == 0) return kColWhite;
  if (diff >= 1 && diff <= 2) return kColYellow;
  if (diff >= 3) return kColRed;

  // diff <= -1: the green / light-blue thresholds widen with the viewer's level. Table of
  // {max viewer level, green if diff<=g, light-blue if diff<=lb, else blue}. When lb==g the
  // light-blue band is empty (low levels have no light-blue con).
  struct Band {
    int max_level, green, light_blue;
  };
  static const Band kBands[] = {
      {7, -4, -4},   {8, -5, -4},   {12, -6, -4},  {16, -7, -5},  {20, -8, -6},  {24, -9, -7},
      {28, -10, -8}, {30, -11, -9}, {32, -12, -9}, {36, -13, -10}, {40, -14, -11}, {44, -16, -12},
      {48, -17, -13}, {52, -18, -14}, {54, -19, -15}, {56, -20, -15}, {60, -21, -16}, {61, -19, -14},
      {62, -17, -12},
  };
  int green = -16, light_blue = -11;  // Default band (viewer level 63+).
  for (const auto &b : kBands) {
    if (my_level <= b.max_level) {
      green = b.green;
      light_blue = b.light_blue;
      break;
    }
  }
  if (diff <= green) return kColGreen;
  if (diff <= light_blue) return kColLightBlue;
  return kColBlue;
}

// State color for the "colors" mode. Returns 0 (no custom color) when nothing applies, so
// the caller can fall back to con colors. Mirrors Zeal's get_player_color_index priority
// (group/raid highlighting is deferred to N1b - those still fall through to guild colors).
static int state_color(char *ent, uint8_t type) {
  if (type == kTypeCorpse) return kColCorpse;
  if (type != kTypePlayer) return 0;  // NPCs: pet group/raid coloring comes in N1b.

  if (*reinterpret_cast<uint8_t *>(ent + kEntPvP)) return kColPVP;
  if (*reinterpret_cast<int *>(ent + kEntAFK)) return kColAFK;
  if (*reinterpret_cast<uint8_t *>(ent + kEntLinkdead)) return kColLD;
  if (*reinterpret_cast<uint8_t *>(ent + kEntLFG)) return kColLFG;
  if (*reinterpret_cast<int *>(ent + kEntAnon) == 2) return kColRole;  // roleplay

  const int guild = *reinterpret_cast<int32_t *>(ent + kEntGuildID);
  if (guild == -1) return kColAdventurer;
  void *self = *kSelf;
  const int my_guild = self ? *reinterpret_cast<int32_t *>(static_cast<char *>(self) + kEntGuildID) : -1;
  return (guild == my_guild) ? kColMyGuild : kColOtherGuild;
}

// Pulses a color toward dim and back (~1.2s period) for the blinking target highlight.
// Self-contained (own clock); shares no state with the client's fade.
static int blink(int rgb) {
  const float phase = (GetTickCount() % 1200) / 1200.0f;
  const float f = 0.4f + 0.6f * (0.5f + 0.5f * std::cos(phase * 6.2831853f));  // 0.4 .. 1.0
  const int r = static_cast<int>(((rgb >> 16) & 0xff) * f);
  const int g = static_cast<int>(((rgb >> 8) & 0xff) * f);
  const int b = static_cast<int>((rgb & 0xff) * f);
  return (r << 16) | (g << 8) | b;
}

// Applies an 0xRRGGBB color to the entity's name sprite via the client's own path:
// actor->vtable[+0x190](colorPtr). The client builds the color as three bytes on its stack
// at [esp+0xc..0xe] with alpha at [+0xf] - i.e. D3DCOLOR little-endian order **B, G, R, A**.
// (Confirmed in-game: writing R,G,B rendered a red con name as blue.)
static void apply_tint(void *actor, int rgb) {
  unsigned char c[4];
  c[0] = static_cast<unsigned char>(rgb & 0xff);          // B
  c[1] = static_cast<unsigned char>((rgb >> 8) & 0xff);   // G
  c[2] = static_cast<unsigned char>((rgb >> 16) & 0xff);  // R
  c[3] = 0xff;                                            // A
  void **vt = *reinterpret_cast<void ***>(actor);
  reinterpret_cast<void(__thiscall *)(void *, void *)>(vt[kActorSetTintVtableIndex])(actor, c);
}

// Computes + applies our tint for one entity. Returns true if we applied a custom color
// (caller then reports the tint as handled), false to let the client's own tint run.
static bool handle_tint(void *entity) {
  if (!g_con_colors && !g_state_colors && !g_target_color) return false;  // Fast bail: nothing enabled.
  if (!entity) return false;
  void *actor = *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor);
  if (!actor) return false;  // Mirror the client's own entry guard (no actor -> it returns 0).

  char *ent = static_cast<char *>(entity);
  const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
  void *target = *kTarget;
  void *self = *kSelf;

  int rgb = 0;  // 0 = no custom color chosen.
  bool is_target = (entity == target);
  if (g_target_color && is_target) {
    rgb = g_target_blink ? blink(kColTarget) : kColTarget;
  }
  if (!rgb && g_state_colors) rgb = state_color(ent, type);
  if (!rgb && g_con_colors && (type == kTypePlayer || type == kTypeNPC) && entity != self) {
    const int my_level = self ? *reinterpret_cast<uint8_t *>(static_cast<char *>(self) + kEntLevel) : 0;
    rgb = con_color(my_level, *reinterpret_cast<uint8_t *>(ent + kEntLevel));
  }
  if (!rgb) return false;  // Nothing to override; fall through to the client default.

  apply_tint(actor, rgb);
  return true;
}

typedef int(__fastcall *TintFn)(void *self, int edx);
static TintFn g_orig_tint = nullptr;

static void maybe_refresh_text(void *entity);  // Defined below; drives forced text rebuilds.
static void ensure_setstr_hook(void *entity);  // Defined below; lazily detours the text setter.

// SetNameSpriteTint detour. __thiscall(this) arrives via __fastcall (this=ecx, unused edx).
// This fires every frame for each visible nameplate, so it's also where we poll for changes
// that need a text rebuild (target/self/HP), which the client itself won't rebuild promptly.
static int __fastcall SetNameSpriteTint_hk(void *self, int edx) {
  // Guard OUR tint computation (the indirect actor-vtable apply + entity field reads):
  // a stale entity/actor should skip our override for this call, not crash the client.
  bool applied = false;
  rcp_guard::run("nameplate.tint", [&] { applied = handle_tint(self); });
  const int ret = applied ? 1 : g_orig_tint(self, edx);  // 1 == a tint was applied (client contract).

  // Lazily install the text-setter detour once we have a live actor (address is in eqgfx).
  if (!g_setstr_installed) rcp_guard::run("nameplate.install", [&] { ensure_setstr_hook(self); });

  // Drive forced text rebuilds. Skip when g_refreshing (our own rebuild re-enters this hook via
  // the nested SetNameSpriteTint). rcp_guard::run always returns here (it swallows faults rather
  // than long-jumping past us), so the guard flag is reliably cleared even if the rebuild faults.
  if (!g_refreshing) {
    g_refreshing = true;
    rcp_guard::run("nameplate.refresh", [&] { maybe_refresh_text(self); });
    g_refreshing = false;
  }
  return ret;
}

// Returns the entity's HP percent (0..100). Uses cur/max when max is known; for NPCs the
// client often carries only a percent in HpCurrent (max 0), so fall back to that.
static int hp_percent(char *ent, uint8_t /*type*/) {
  const int cur = *reinterpret_cast<int *>(ent + kEntHPCurrent);
  const int mx = *reinterpret_cast<int *>(ent + kEntHPMax);
  int pct = (mx > 0) ? (cur * 100 / mx) : cur;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// Maps an actor pointer back to its entity (self/target) by comparing actor pointers, then
// transforms the nameplate text for the enabled N2 options. Returns a pointer into a static
// buffer, or nullptr to leave `text` unchanged. POD-bodied (static char buffers, no std::string)
// so it is safe inside an rcp_guard frame.
static const char *transform_by_actor(void *actor, const char *text) {
  if (!g_target_marker && !g_target_health && !g_hide_self) return nullptr;  // Fast bail.
  if (!actor || !text) return nullptr;

  void *target = *kTarget;
  void *self = *kSelf;
  void *target_actor = target ? *reinterpret_cast<void **>(static_cast<char *>(target) + kEntActor) : nullptr;
  void *self_actor = self ? *reinterpret_cast<void **>(static_cast<char *>(self) + kEntActor) : nullptr;
  const bool is_target = (target_actor && actor == target_actor);
  const bool is_self = (self_actor && actor == self_actor);

  if (g_text_log > 0 && target) {  // Only spend the budget while a target exists.
    logger::logf("[nameplate] xf actor=%p tgt_actor=%p self_actor=%p is_t=%d is_s=%d in='%s'", actor, target_actor,
                 self_actor, (int)is_target, (int)is_self, text ? text : "(null)");
    --g_text_log;
  }

  // Hide your own nameplate (but never when it's your current target).
  if (g_hide_self && is_self && !is_target) {
    static char empty[1] = {0};
    return empty;
  }

  // Target embellishments: >>Name  NN%<<  (matches Zeal's ordering).
  if (is_target && (g_target_marker || g_target_health)) {
    char *ent = static_cast<char *>(target);
    const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
    static char buf[256];
    const char *pre = g_target_marker ? ">>" : "";
    const char *post = g_target_marker ? "<<" : "";
    if (g_target_health && (type == kTypePlayer || type == kTypeNPC))
      std::snprintf(buf, sizeof(buf), "%s%s %d%%%s", pre, text, hp_percent(ent, type), post);
    else
      std::snprintf(buf, sizeof(buf), "%s%s%s", pre, text, post);
    return buf;
  }
  return nullptr;
}

// True if the entity's name sprite is currently shown (honors the client's shownames state).
static bool name_shown(void *actor) {
  void **vt = *reinterpret_cast<void ***>(actor);
  return reinterpret_cast<char(__thiscall *)(void *, int)>(vt[kActorNameShownVtableIndex])(actor, 0) != 0;
}

// Re-drives the client's SetNameSpriteState so our text transform (and tint) re-run now. Only
// rebuilds when the name is already shown, so we never create a nameplate the user turned off.
// g_refreshing is set by the CALLER around this so the re-entrant tint hook skips its refresh.
static void force_state(void *entity) {
  void *actor = *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor);
  if (!actor) return;
  const int show = name_shown(actor) ? 1 : 0;
  // SetNameSpriteState early-returns (0x58E2DD) when bDisplayNameSprite (+0x1228) is 0, which it
  // is for NPCs after their spawn-time name set - so a forced rebuild did nothing for mob targets
  // (self's flag stays 1, which is why hideself worked). Set it so the rebuild proceeds.
  uint8_t *display_flag = reinterpret_cast<uint8_t *>(static_cast<char *>(entity) + kEntDisplayNameSprite);
  if (g_refresh_log > 0) {
    logger::logf("[nameplate] force rebuild entity=%p show=%d disp=%d", entity, show, (int)*display_flag);
    --g_refresh_log;
  }
  *display_flag = 1;
  reinterpret_cast<int(__thiscall *)(void *, int)>(kSetNameSpriteState)(entity, show);
}

// Per-frame (from the tint hook) check: if this entity is our target or self and the thing we
// modify changed (target identity, target HP%, or a hideself toggle), force a text rebuild.
// POD-bodied for rcp_guard.
static void maybe_refresh_text(void *entity) {
  if (!g_target_marker && !g_target_health && !g_hide_self) return;  // Fast bail.
  if (!entity) return;

  void *target = *kTarget;

  // Diagnostic: dump the pointers for the first frames after a toggle so we can see whether the
  // target's tint even reaches here (entity == target) and what the target pointer actually is.
  if (g_diag > 0 && target) {  // Only spend the budget while a target exists.
    logger::logf("[nameplate] mrt entity=%p target=%p self=%p match_t=%d match_s=%d", entity, target, *kSelf,
                 (int)(entity == target), (int)(entity == *kSelf));
    --g_diag;
  }

  // Detect a target change (works even when the new target is null): the previous target needs
  // one cleanup rebuild to strip our marker/health text.
  if (target != g_last_target) {
    if (g_last_target != reinterpret_cast<void *>(-1)) g_prev_target = g_last_target;
    g_last_target = target;
    g_last_target_hp = -1;
  }

  // Strip our text off the entity that just stopped being the target.
  if (entity == g_prev_target && (g_target_marker || g_target_health)) {
    g_prev_target = nullptr;
    force_state(entity);
    return;
  }

  // Current target: (re)apply marker/health, and refresh whenever its HP% changes.
  if (entity == target && (g_target_marker || g_target_health)) {
    const int hp = g_target_health ? hp_percent(static_cast<char *>(entity), 0) : 0;
    if (hp != g_last_target_hp) {
      g_last_target_hp = hp;
      force_state(entity);
      return;
    }
  }

  // Self: rebuild once after a hideself toggle (thereafter the client's own rebuilds re-blank it).
  if (entity == *kSelf && g_hide_self && g_self_dirty) {
    g_self_dirty = false;
    force_state(entity);
  }
}

// Actor set-string detour: __thiscall(actor, int flag, char* text, char* scratch), the eqgfx
// method at actor vtable +0x18c. We swap in our transformed text; the callee copies it, so our
// static buffer only needs to outlive the forwarded call.
typedef int(__fastcall *SetStrFn)(void *actor, int edx, int flag, char *text, char *scratch);
static SetStrFn g_orig_setstr = nullptr;

static int __fastcall ActorSetString_hk(void *actor, int edx, int flag, char *text, char *scratch) {
  char *out = text;
  char *out_scratch = scratch;
  rcp_guard::run("nameplate.text", [&] {
    const char *m = transform_by_actor(actor, text);
    if (m) {
      out = const_cast<char *>(m);
      // The two string params are NOT redundant: the client's clear path passes (NULL, "")
      // and its set path passes (name, copyOfName). param2 acts as presence/identity (empty ->
      // remove sprite, which is why hideself worked via param2 alone) while param3 is the text
      // the renderer draws - so the transform must be applied to BOTH or markers never show.
      static char copy[256];
      std::snprintf(copy, sizeof(copy), "%s", m);
      out_scratch = copy;
    }
  });
  return g_orig_setstr(actor, edx, flag, out, out_scratch);
}

// Lazily detours the actor's set-string method the first time we have a live actor (its address
// lives in eqgfx and isn't known until an actor exists). Only installs once a text option is on.
static void ensure_setstr_hook(void *entity) {
  if (g_setstr_installed || !entity) return;
  if (!g_target_marker && !g_target_health && !g_hide_self) return;
  void *actor = *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor);
  if (!actor) return;
  void **vt = *reinterpret_cast<void ***>(actor);
  void *fn = vt[kActorSetStringVtableIndex];
  if (!fn || reinterpret_cast<uintptr_t>(fn) < 0x10000) return;  // Sanity: real code address.
  auto rcp = RcpService::get_instance();
  if (!rcp || !rcp->hooks) return;
  rcp->hooks->Add("nameplate_setstr", static_cast<int>(reinterpret_cast<uintptr_t>(fn)), ActorSetString_hk,
                  hook_type_detour);
  g_orig_setstr = rcp->hooks->hook_map["nameplate_setstr"]->original(ActorSetString_hk);
  g_setstr_installed = true;
  logger::logf("[nameplate] actor set-string (vtable+0x18c) hooked at 0x%x", (unsigned)reinterpret_cast<uintptr_t>(fn));
}

static void print_status() {
  char msg[256];
  std::snprintf(msg, sizeof(msg),
                "rof2ClientPlus nameplates: concolors=%s | colors=%s | targetcolor=%s | targetblink=%s | "
                "targetmarker=%s | targethealth=%s | hideself=%s",
                g_con_colors ? "on" : "off", g_state_colors ? "on" : "off", g_target_color ? "on" : "off",
                g_target_blink ? "on" : "off", g_target_marker ? "on" : "off", g_target_health ? "on" : "off",
                g_hide_self ? "on" : "off");
  Rcp::Game::print_chat(msg);
}

// Toggles a single bool option. If args[2] is on/off/1/0 use it, otherwise flip.
static void set_option(bool &opt, const std::vector<std::string> &args) {
  if (args.size() >= 3 && (args[2] == "on" || args[2] == "1"))
    opt = true;
  else if (args.size() >= 3 && (args[2] == "off" || args[2] == "0"))
    opt = false;
  else
    opt = !opt;
}

NamePlate::NamePlate(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  // Install the tint detour now. It bails immediately while every option is off, so it is
  // safe at load time and only does work once the user opts in.
  rcp->hooks->Add("nameplate_tint", static_cast<int>(kSetNameSpriteTint), SetNameSpriteTint_hk, hook_type_detour);
  g_orig_tint = rcp->hooks->hook_map["nameplate_tint"]->original(SetNameSpriteTint_hk);
  logger::logf("[nameplate] SetNameSpriteTint hooked at 0x%x (concolors=%d colors=%d targetcolor=%d targetblink=%d)",
               (unsigned)kSetNameSpriteTint, (int)g_con_colors, (int)g_state_colors, (int)g_target_color,
               (int)g_target_blink);

  // The text-setter detour (N2) installs lazily from the tint hook once a live actor exists
  // (its address is in eqgfx). See ensure_setstr_hook. Nothing to install here.
  logger::logf("[nameplate] text mods (targetmarker=%d targethealth=%d hideself=%d); setter hook installs in-game",
               (int)g_target_marker, (int)g_target_health, (int)g_hide_self);

  // If text options were already enabled from the ini, refresh on first sight of target/self.
  if (g_target_marker || g_target_health || g_hide_self) mark_text_dirty();

  rcp->commands_hook->Add(
      "/rcpnameplate", {"/rcpname"},
      "Nameplate options. '/rcpnameplate <concolors|colors|targetcolor|targetblink|targetmarker|targethealth|"
      "hideself> [on|off]'.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &opt = args[1];
          if (opt == "concolors")
            set_option(g_con_colors, args);
          else if (opt == "colors")
            set_option(g_state_colors, args);
          else if (opt == "targetcolor")
            set_option(g_target_color, args);
          else if (opt == "targetblink")
            set_option(g_target_blink, args);
          else if (opt == "targetmarker")
            set_option(g_target_marker, args);
          else if (opt == "targethealth")
            set_option(g_target_health, args);
          else if (opt == "hideself")
            set_option(g_hide_self, args);
          else {
            Rcp::Game::print_chat(
                "Usage: /rcpnameplate <concolors|colors|targetcolor|targetblink|targetmarker|targethealth|hideself> "
                "[on|off]");
            return true;
          }
          // A text-option change won't show until the client rebuilds the nameplate, so force
          // a rebuild of the target + self on the next frame.
          if (opt == "targetmarker" || opt == "targethealth" || opt == "hideself") mark_text_dirty();
          save_settings();
        }
        print_status();
        return true;
      });
  logger::log("[nameplate] /rcpnameplate registered");
}
