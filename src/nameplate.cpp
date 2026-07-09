// rof2ClientPlus - character nameplate tinting + text mods (Zeal nameplate port, N1+N2), RoF2.
// See nameplate.h and PORTING_NOTES.md (Nameplate) for the mechanism + full RE.
#include "nameplate.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
// CGuild::GetGuildName(this, int guildId) -> const char*, __thiscall. The guild manager is a
// static object AT 0xDD5CF8 (callers use `mov ecx, 0xDD5CF8` immediate, not a pointer deref).
static constexpr uintptr_t kGetGuildName = 0x425670;
static void *const kGuildInstance = reinterpret_cast<void *>(0xDD5CF8);
// __ShowNames: the /shownames level (int). 0=off, 1..6 accepted natively (>3 => title+suffix).
static int *const kShowNamesLevel = reinterpret_cast<int *>(0xDE0A70);
// Group + raid membership sources (both are objects at these addresses, not pointers).
// CRaid object (instCRaid): raidMembers[72] @ +0x260 (each 0x94, Name@0x00), count @ +0x2c04.
static char *const kRaid = reinterpret_cast<char *>(0xDD2690);
static constexpr int kRaidMembers = 0x260;
static constexpr int kRaidMemberStride = 0x94;
static constexpr int kRaidMemberCount = 0x2c04;
static constexpr int kRaidMaxMembers = 72;
// Group: pLocalPC (PcClient, *(void**)0xDD261C) -> CGroup* Group @ +0x31cc; CGroup holds
// CGroupMember*[6] @ +0x04, each CGroupMember has pPlayer (PlayerClient*) @ +0x28. So we can
// match group members by SPAWN POINTER, not name (the LabelCache name approach didn't populate).
static void **const kLocalPC = reinterpret_cast<void **>(0xDD261C);
static constexpr int kPcGroup = 0x31cc;
static constexpr int kGroupMembers = 0x04;
static constexpr int kGroupMemberPlayer = 0x28;
static constexpr int kGroupMaxMembers = 6;
static void **const kSelf = reinterpret_cast<void **>(0xDD2630);    // local player
static void **const kTarget = reinterpret_cast<void **>(0xDD2648);  // current target

// PlayerClient (Entity) field offsets.
static constexpr int kEntActor = 0x101c;   // CActorInterface* (all-virtual); apply tint via its vtable
static constexpr int kEntLastname = 0x38;  // char[0x20]
static constexpr int kEntDisplayedName = 0xe4;  // char[]; pre-trimmed name ("Priest of Discord" / first name)
static constexpr int kEntGM = 0x25c;        // char: nonzero = GM / game master (eqlib-confirmed vs our offsets)
static constexpr int kEntType = 0x125;     // uint8: 0=Player, 1=NPC, 2=Corpse
static constexpr int kEntLevel = 0x250;    // uint8
static constexpr int kEntAnon = 0x2b8;     // int: 1=anonymous, 2=roleplay
static constexpr int kEntHPMax = 0x2dc;    // int32
static constexpr int kEntHPCurrent = 0x2e4; // int32 (for NPCs the client only knows a percent)
static constexpr int kEntPvP = 0x349;      // uint8 (bool) PvPFlag
static constexpr int kEntGuildID = 0x34c;  // int32, -1 = unguilded
static constexpr int kEntTitle = 0x3dd;    // char[0x20]; player title prefix (server-resolved string)
static constexpr int kEntSuffix = 0x3fe;   // char[0x20]; player suffix (server-resolved string)
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

// ---- Nameplate color palette (0xRRGGBB). Each role is user-editable from the options window
// (N6); defaults preserve the original hardcoded look, and the con colors mirror the get_con_*
// defaults in game_functions.cpp. Persisted to rof2ClientPlus.ini [NameplateColors]. The three
// parallel arrays below are all indexed by NpColorRole - keep them in the same order. ----
enum NpColorRole {
  kRoleConEven = 0,   // NPC same level
  kRoleConYellow,     // NPC 1-2 above
  kRoleConRed,        // NPC 3+ above
  kRoleConGreen,      // NPC far below
  kRoleConLightBlue,  // NPC below
  kRoleConBlue,       // NPC slightly below
  kRoleTarget,        // current target
  kRolePVP,           // PvP-flagged player
  kRoleAFK,           // AFK player
  kRoleLD,            // linkdead player
  kRoleLFG,           // looking-for-group player
  kRoleGroup,         // group member
  kRoleRaid,          // raid member
  kRoleRoleplay,      // roleplaying player
  kRoleMyGuild,       // your guild
  kRoleCorpse,        // corpse
  kRoleGM,            // GM / game master (green)
  kRolePlayer,        // default non-special player color
  kNpColorCount
};
// (No "other guild" role: players with no special state keep the client's default color.)

static int g_colors[kNpColorCount] = {
    0xf0f0f0,  // ConEven (white)
    0xf0f000,  // ConYellow
    0xf00000,  // ConRed
    0x00f000,  // ConGreen
    0x00f0f0,  // ConLightBlue
    0x0040f0,  // ConBlue
    0xff8000,  // Target (orange)
    0xf00000,  // PVP
    0x808080,  // AFK
    0x606060,  // LD
    0xf0f000,  // LFG
    0x40a0ff,  // Group (sky blue)
    0xb060ff,  // Raid (purple)
    0xf000f0,  // Roleplay
    0x00f000,  // MyGuild
    0x909090,  // Corpse
    0x00c800,  // GM (green 200)
    0x4060ff,  // Player (default non-special player - a deeper blue than the con light-blue)
};

// Human labels for the options-window color buttons (index == NpColorRole).
// MUST stay in sync with ROLES in tools/gen_rcp_options_ui.py (same order).
static const char *const kNpColorNames[kNpColorCount] = {
    "Con: even", "Con: yellow", "Con: red", "Con: green", "Con: light blue", "Con: blue",
    "Target",    "PvP",         "AFK",      "Linkdead",   "LFG",             "Group",
    "Raid",      "Roleplay",    "My guild", "Corpse",     "GM",              "Player",
};

// ini keys under [NameplateColors] (index == NpColorRole). Stored as RRGGBB hex.
static const char *const kNpColorIniKeys[kNpColorCount] = {
    "ConEven", "ConYellow", "ConRed", "ConGreen", "ConLightBlue", "ConBlue", "Target", "PVP", "AFK",
    "LD",      "LFG",       "Group",  "Raid",     "Roleplay",     "MyGuild", "Corpse", "GM",  "Player",
};
static constexpr char kIniColorSection[] = "NameplateColors";

// Parses "RRGGBB" (optionally "#RRGGBB") hex into an 0xRRGGBB int (masked to 24 bits).
static int parse_hex_color(const std::string &s) {
  const char *p = s.c_str();
  if (*p == '#') ++p;
  return static_cast<int>(std::strtoul(p, nullptr, 16) & 0xffffff);
}

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
// Target-blink period in ms (full dim-and-back cycle). Shared by every coloring mode.
static int g_blink_ms = 1200;
// Name generation (N3) is ALWAYS active (was an option; the generated names mirror the client's
// look at /shownames 1-4 and unlock the extended 5-7 combos, so there is no reason to opt out).
static int g_shownames_level = -1;    // Captured from /shownames (0-7); -1 = mirror the client's level.

// Forced text-rebuild bookkeeping (N2). The client rebuilds nameplate text lazily, so we
// re-drive SetNameSpriteState ourselves when the thing we modify changes.
static bool g_setstr_installed = false;              // Whether the actor set-string detour is installed yet.
static bool g_refreshing = false;                    // Recursion guard (our forced rebuild re-enters the tint hook).
static void *g_current_entity = nullptr;             // Entity whose name SetNameSpriteState is currently building.
static void *g_last_target = reinterpret_cast<void *>(-1);  // Target we last refreshed (-1 = force on first sight).
static void *g_prev_target = nullptr;                // Prior target awaiting a cleanup rebuild (strip its marker).
static int g_last_target_hp = -1;                    // Target HP% we last rendered (for targethealth).
static bool g_self_dirty = false;                    // Self nameplate needs one rebuild (toggle of hideself).

// Ring of player actors we've already rebuilt once with generated names (so a /shownames
// change refreshes each player a single time, not every frame). Cleared to force a full refresh.
static void *g_player_refreshed[64] = {nullptr};
static int g_player_ring_pos = 0;
static bool player_recently_refreshed(void *actor) {
  for (int i = 0; i < 64; ++i)
    if (g_player_refreshed[i] == actor) return true;
  return false;
}
static void mark_player_refreshed(void *actor) {
  g_player_refreshed[g_player_ring_pos] = actor;
  g_player_ring_pos = (g_player_ring_pos + 1) % 64;
}
static void clear_player_ring() {
  for (int i = 0; i < 64; ++i) g_player_refreshed[i] = nullptr;
  g_player_ring_pos = 0;
}

// Marks text state dirty so the next frame re-pushes the target + self (+ all players) nameplates.
// Called on any text-option toggle so the change is visible without waiting for the client.
static void mark_text_dirty() {
  g_last_target = reinterpret_cast<void *>(-1);
  g_last_target_hp = -1;
  g_self_dirty = true;
  clear_player_ring();
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
  if (ini.exists(kIniSection, "BlinkMs")) g_blink_ms = ini.getValue<int>(kIniSection, "BlinkMs");
  if (g_blink_ms < 200) g_blink_ms = 200;
  if (g_blink_ms > 3000) g_blink_ms = 3000;
  for (int i = 0; i < kNpColorCount; ++i)
    if (ini.exists(kIniColorSection, kNpColorIniKeys[i]))
      g_colors[i] = parse_hex_color(ini.getValue<std::string>(kIniColorSection, kNpColorIniKeys[i]));
}

// Persists a single palette entry as RRGGBB hex under [NameplateColors].
static void save_color(int role) {
  if (role < 0 || role >= kNpColorCount) return;
  IO_ini ini(IO_ini::kRcpIniFilename);
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%06X", g_colors[role] & 0xffffff);
  ini.setValue<std::string>(kIniColorSection, kNpColorIniKeys[role], buf);
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
  ini.setValue<int>(kIniSection, "BlinkMs", g_blink_ms);
}

namespace nameplate_settings {
bool get_con_colors() { return g_con_colors; }
bool get_state_colors() { return g_state_colors; }
bool get_target_color() { return g_target_color; }
bool get_target_blink() { return g_target_blink; }
bool get_target_marker() { return g_target_marker; }
bool get_target_health() { return g_target_health; }
bool get_hide_self() { return g_hide_self; }
int get_blink_ms() { return g_blink_ms; }
void set_blink_ms(int ms) {
  g_blink_ms = ms < 200 ? 200 : (ms > 3000 ? 3000 : ms);
  save_settings();
}
void set(bool con_colors, bool state_colors, bool target_color, bool target_blink, bool target_marker,
         bool target_health, bool hide_self) {
  // A text-affecting option (marker/health/hideself) only becomes visible once the client
  // rebuilds the nameplate, so force a refresh if any of them changed - matching what the
  // /rcpnameplate command does. Color options apply through the per-frame tint hook and
  // need no rebuild.
  const bool text_changed =
      target_marker != g_target_marker || target_health != g_target_health || hide_self != g_hide_self;
  g_con_colors = con_colors;
  g_state_colors = state_colors;
  g_target_color = target_color;
  g_target_blink = target_blink;
  g_target_marker = target_marker;
  g_target_health = target_health;
  g_hide_self = hide_self;
  save_settings();
  if (text_changed) mark_text_dirty();
}
}  // namespace nameplate_settings

namespace nameplate_colors {
int count() { return kNpColorCount; }
const char *name(int role) { return (role >= 0 && role < kNpColorCount) ? kNpColorNames[role] : ""; }
int get(int role) { return (role >= 0 && role < kNpColorCount) ? g_colors[role] : 0; }
void set(int role, int rgb) {
  if (role < 0 || role >= kNpColorCount) return;
  g_colors[role] = rgb & 0xffffff;
  save_color(role);
  // Con/state/target tints are re-applied every frame from the tint hook, so the new color shows
  // immediately - no text rebuild needed.
}
}  // namespace nameplate_colors

// ---- Con color: reproduces the level-band table from Rcp::Game::GetLevelCon locally so
// this module stays self-contained (that function reaches through get_user_color / the
// unconstructed options UI, which is unsafe in this build). Returns an 0xRRGGBB color. ----
static int con_color(int my_level, int ent_level) {
  const int diff = ent_level - my_level;
  if (diff == 0) return g_colors[kRoleConEven];
  if (diff >= 1 && diff <= 2) return g_colors[kRoleConYellow];
  if (diff >= 3) return g_colors[kRoleConRed];

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
  if (diff <= green) return g_colors[kRoleConGreen];
  if (diff <= light_blue) return g_colors[kRoleConLightBlue];
  return g_colors[kRoleConBlue];
}

// State color for the "colors" mode. Returns 0 (no custom color) when nothing applies, so
// the caller can fall back to con colors. Mirrors Zeal's get_player_color_index priority
// (group/raid highlighting is deferred to N1b - those still fall through to guild colors).
// Case-insensitive compare of an entity's (clean) name to another name; false if `other` empty.
static bool name_matches(char *ent, const char *other) {
  return other[0] && _stricmp(static_cast<char *>(ent) + kEntDisplayedName, other) == 0;
}

// True if the entity is in the local player's group. Walks the CGroup member list and matches by
// spawn pointer. Self is a member whenever a group exists (its slot may or may not be present).
static bool is_group_member(char *ent) {
  void *pc = *kLocalPC;
  if (!pc) return false;
  void *group = *reinterpret_cast<void **>(static_cast<char *>(pc) + kPcGroup);
  if (!group) return false;  // No group object at all.
  void *self = *kSelf;
  // A group only "counts" if it has at least one member besides you - the client keeps a stale
  // (solo) group struct around, which otherwise colored your own plate as grouped.
  int others = 0;
  bool ent_is_member = false;
  for (int i = 0; i < kGroupMaxMembers; ++i) {
    void *member = *reinterpret_cast<void **>(static_cast<char *>(group) + kGroupMembers + i * 4);
    if (!member) continue;
    void *mp = *reinterpret_cast<void **>(static_cast<char *>(member) + kGroupMemberPlayer);
    if (!mp) continue;
    if (mp != self) ++others;
    if (mp == static_cast<void *>(ent)) ent_is_member = true;
  }
  if (others == 0) return false;                        // Solo/empty group => not grouped.
  return (static_cast<void *>(ent) == self) || ent_is_member;
}

// True if the entity is in the local player's raid (name match against CRaid's member list).
static bool is_raid_member(char *ent) {
  const int count = *reinterpret_cast<int *>(kRaid + kRaidMemberCount);
  if (count <= 0) return false;
  for (int i = 0; i < kRaidMaxMembers; ++i) {
    const char *mname = kRaid + kRaidMembers + i * kRaidMemberStride;  // RaidMember.Name@0x00
    if (name_matches(ent, mname)) return true;
  }
  return false;
}

static int state_color(char *ent, uint8_t type) {
  if (type == kTypeCorpse) return g_colors[kRoleCorpse];
  if (type != kTypePlayer) return 0;  // NPC pet group/raid coloring is a later step.

  if (*reinterpret_cast<char *>(ent + kEntGM)) return g_colors[kRoleGM];  // GMs are green (highest priority).
  if (*reinterpret_cast<uint8_t *>(ent + kEntPvP)) return g_colors[kRolePVP];
  if (*reinterpret_cast<int *>(ent + kEntAFK)) return g_colors[kRoleAFK];
  if (*reinterpret_cast<uint8_t *>(ent + kEntLinkdead)) return g_colors[kRoleLD];
  if (*reinterpret_cast<uint8_t *>(ent + kEntLFG)) return g_colors[kRoleLFG];
  if (is_group_member(ent)) return g_colors[kRoleGroup];  // Group takes priority over raid (Zeal order).
  if (is_raid_member(ent)) return g_colors[kRoleRaid];
  if (*reinterpret_cast<int *>(ent + kEntAnon) == 2) return g_colors[kRoleRoleplay];  // roleplay

  const int guild = *reinterpret_cast<int32_t *>(ent + kEntGuildID);
  if (guild == -1) return 0;  // Unguilded, no special state: keep the client's default color.
  void *self = *kSelf;
  const int my_guild = self ? *reinterpret_cast<int32_t *>(static_cast<char *>(self) + kEntGuildID) : -1;
  // Only YOUR guild gets a highlight; other players keep the client's default color.
  return (guild == my_guild) ? g_colors[kRoleMyGuild] : 0;
}

// Pulses a color toward dim and back for the blinking target highlight. The full cycle
// takes g_blink_ms (user-set, shared by every coloring mode). Self-contained (own clock);
// replaces the client's fade, which for targeted PCs is far too rapid.
static int blink(int rgb) {
  const unsigned period = static_cast<unsigned>(g_blink_ms);
  const float phase = (GetTickCount() % period) / static_cast<float>(period);
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
  if (!g_con_colors && !g_state_colors && !g_target_color && !g_target_blink)
    return false;  // Fast bail: nothing enabled.
  if (!entity) return false;
  void *actor = *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor);
  if (!actor) return false;  // Mirror the client's own entry guard (no actor -> it returns 0).

  char *ent = static_cast<char *>(entity);
  const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
  void *target = *kTarget;
  void *self = *kSelf;

  int rgb = 0;  // 0 = no custom color chosen.
  bool is_target = (entity == target);
  if (g_target_color && is_target) rgb = g_colors[kRoleTarget];
  if (!rgb && g_state_colors) rgb = state_color(ent, type);
  if (!rgb && g_con_colors && type == kTypeNPC) {  // Con colors apply to NPCs only, not players.
    const int my_level = self ? *reinterpret_cast<uint8_t *>(static_cast<char *>(self) + kEntLevel) : 0;
    rgb = con_color(my_level, *reinterpret_cast<uint8_t *>(ent + kEntLevel));
  }

  // Target blink applies in EVERY mode, PCs and NPCs alike: pulse whatever color the target's
  // nameplate would otherwise show. When no coloring mode claims it, pulse an approximation of
  // the client's default (cyan players / white NPCs / gray corpses) - taking the tint over also
  // replaces the client's own too-rapid target fade.
  if (g_target_blink && is_target) {
    if (!rgb) rgb = (type == kTypePlayer) ? 0x00f0f0 : (type == kTypeCorpse ? 0x909090 : 0xf0f0f0);
    rgb = blink(rgb);
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

// Bounded append of s into buf[pos]. POD helper for building generated names inside rcp_guard.
static void append_str(char *buf, int size, int &pos, const char *s) {
  while (*s && pos < size - 1) buf[pos++] = *s++;
  buf[pos] = 0;
}

// Shownames component rules (Zeal's mapping): 1=first, 2=first+last, 3=+guild,
// 4=title+first+last+guild, 5=title+first, 6=title+first+last, 7=first+guild.
static bool show_title(int lvl) { return lvl == 4 || lvl == 5 || lvl == 6; }
static bool show_last(int lvl) { return lvl == 2 || lvl == 3 || lvl == 4 || lvl == 6; }
static bool show_guild(int lvl) { return lvl == 3 || lvl == 4 || lvl == 7; }

// Generates a player's nameplate name line from entity fields per the shownames level, into a
// static buffer (single line; guild inline). POD-bodied for rcp_guard.
static const char *generate_player_name(char *ent) {
  const int lvl = (g_shownames_level >= 0) ? g_shownames_level : *kShowNamesLevel;
  static char buf[192];
  int pos = 0;
  buf[0] = 0;

  if (show_title(lvl)) {
    const char *title = ent + kEntTitle;
    if (title[0]) {
      append_str(buf, sizeof(buf), pos, title);
      append_str(buf, sizeof(buf), pos, " ");
    }
  }
  append_str(buf, sizeof(buf), pos, ent + kEntDisplayedName);  // First name (pre-trimmed).
  if (show_last(lvl)) {
    const char *last = ent + kEntLastname;
    if (last[0]) {
      append_str(buf, sizeof(buf), pos, " ");
      append_str(buf, sizeof(buf), pos, last);
    }
  }
  if (show_guild(lvl)) {
    const int guild_id = *reinterpret_cast<int *>(ent + kEntGuildID);
    if (guild_id != -1) {
      const char *gname =
          reinterpret_cast<const char *(__thiscall *)(void *, int)>(kGetGuildName)(kGuildInstance, guild_id);
      if (gname && gname[0]) {
        append_str(buf, sizeof(buf), pos, " <");
        append_str(buf, sizeof(buf), pos, gname);
        append_str(buf, sizeof(buf), pos, ">");
      }
    }
  }
  if (*reinterpret_cast<int *>(ent + kEntAFK)) append_str(buf, sizeof(buf), pos, " AFK");
  if (*reinterpret_cast<uint8_t *>(ent + kEntLinkdead)) append_str(buf, sizeof(buf), pos, " LD");
  if (*reinterpret_cast<uint8_t *>(ent + kEntLFG)) append_str(buf, sizeof(buf), pos, " LFG");
  return buf;
}

// POD cache of the client's full nameplate text per NPC entity, captured at the set-string hook
// (which receives the complete multi-line string - including a pet's owner line and anything else
// the client appends). Used for the billboard so we don't lose any client text. Fixed-size + fixed
// buffers so it is safe to write inside an rcp_guard body (no heap; a longjmp abandons nothing).
static constexpr int kTextCacheSize = 128;
struct NpTextEntry {
  void *entity;
  char text[256];
};
static NpTextEntry g_text_cache[kTextCacheSize] = {};
static int g_text_cache_next = 0;

static void cache_np_text(void *entity, const char *text) {
  int slot = -1;
  for (int i = 0; i < kTextCacheSize; ++i)
    if (g_text_cache[i].entity == entity) {
      slot = i;
      break;
    }
  if (slot < 0) {
    slot = g_text_cache_next;
    g_text_cache_next = (g_text_cache_next + 1) % kTextCacheSize;
    g_text_cache[slot].entity = entity;
  }
  std::snprintf(g_text_cache[slot].text, sizeof(g_text_cache[slot].text), "%s", text);
}

static const char *lookup_np_text(void *entity) {
  for (int i = 0; i < kTextCacheSize; ++i)
    if (g_text_cache[i].entity == entity && g_text_cache[i].text[0]) return g_text_cache[i].text;
  return nullptr;
}

// --- Exposed for the custom-font billboard nameplates (font_overlay, N4c) -----------------
// Text the billboard should draw for an entity: for players, the full generated name line
// (title/first/last/guild/AFK per shownames). For NPCs/corpses, the client's own captured full
// text (keeps the pet-owner line etc.), falling back to the plain display name until it is cached.
std::string nameplate::billboard_text(void *entity) {
  if (!entity) return {};
  char *ent = static_cast<char *>(entity);
  const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
  if (type == kTypePlayer) return generate_player_name(ent);
  if (const char *cached = lookup_np_text(entity)) return cached;
  return std::string(ent + kEntDisplayedName);
}

// Color the billboard name should use: the same con/state palette the tint feature uses (so
// billboards are con-colored like native), always applied (not gated on the tint options),
// with a client-like default (cyan PC / white NPC / gray corpse) when nothing special applies.
int nameplate::billboard_color(void *entity) {
  if (!entity) return 0xffffff;
  char *ent = static_cast<char *>(entity);
  const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
  int rgb = state_color(ent, type);  // guild/group/raid/pvp/afk/ld/lfg/roleplay/corpse
  if (!rgb && type == kTypeNPC) {
    void *self = *kSelf;
    const int my_level = self ? *reinterpret_cast<uint8_t *>(static_cast<char *>(self) + kEntLevel) : 0;
    rgb = con_color(my_level, *reinterpret_cast<uint8_t *>(ent + kEntLevel));
  }
  // Default (no con/state): the customizable Player color for players, Corpse gray for corpses.
  if (!rgb) rgb = (type == kTypePlayer) ? g_colors[kRolePlayer] : (type == kTypeCorpse ? g_colors[kRoleCorpse] : 0xffffff);
  return rgb;
}

// Transforms nameplate text for the enabled options. Uses g_current_entity (set by the
// SetNameSpriteState hook) so we have the full entity, not just the actor. Returns a pointer into
// a static buffer, or nullptr to leave `text` unchanged. POD-bodied for rcp_guard.
static bool g_suppress_native = false;  // Blank ALL native name text (billboard nameplates own the display).

void nameplate::set_suppress_native(bool suppress) { g_suppress_native = suppress; }

// Pure con color for an entity vs the local player, ALWAYS computed (independent of the con-colors
// setting). Uses the same level-band table + user-editable con palette as the nameplates, so the
// target ring's con coloring tracks the Colors tab. Falls back to "even" white if levels are unknown.
int nameplate::con_color_for(void *entity) {
  if (!entity) return g_colors[kRoleConEven];
  void *self = *kSelf;
  const int my_level = self ? *reinterpret_cast<uint8_t *>(static_cast<char *>(self) + kEntLevel) : 0;
  const int ent_level = *reinterpret_cast<uint8_t *>(static_cast<char *>(entity) + kEntLevel);
  return con_color(my_level, ent_level);
}

static const char *transform_entity(void *actor, const char *text) {
  if (!actor || !text) return nullptr;

  // Billboard nameplates active: blank every native name (the client keeps calling the tint hook on
  // its own timer, which re-runs this and keeps them blank; the tint still fires so con-color is
  // unaffected). Returning empty removes the native sprite. First, capture the client's full text
  // for NPCs so the billboard can reproduce it (pet-owner line etc.) - players are generated instead.
  if (g_suppress_native) {
    void *entity = g_current_entity;
    if (entity && text[0] && *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor) == actor &&
        *reinterpret_cast<uint8_t *>(static_cast<char *>(entity) + kEntType) != kTypePlayer)
      cache_np_text(entity, text);
    static char empty[1] = {0};
    return empty;
  }

  void *entity = g_current_entity;
  if (!entity || *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor) != actor) return nullptr;

  char *ent = static_cast<char *>(entity);
  const uint8_t type = *reinterpret_cast<uint8_t *>(ent + kEntType);
  const bool is_target = (entity == *kTarget);
  const bool is_self = (entity == *kSelf);

  // Hide your own nameplate (but never when it's your current target).
  if (g_hide_self && is_self && !is_target) {
    static char empty[1] = {0};
    return empty;
  }

  // For players, regenerate the whole name line from entity fields per shownames level
  // (always on: mirrors the client at levels 1-4, unlocks the extended 5-7 combos).
  const char *base = text;
  if (type == kTypePlayer && *kShowNamesLevel > 0) base = generate_player_name(ent);

  // Target embellishments: >>Name NN%<<. Decorate ONLY the first line (keeps any extra client
  // line, e.g. a pet's "(Owner's Pet)" line, after the "<<") so the marker wraps just the name.
  if (is_target && (g_target_marker || g_target_health)) {
    static char buf[320];
    const char *nl = std::strchr(base, '\n');
    const int name_len = nl ? static_cast<int>(nl - base) : static_cast<int>(std::strlen(base));
    const char *rest = nl ? nl : "";
    const char *pre = g_target_marker ? ">>" : "";
    const char *post = g_target_marker ? "<<" : "";
    if (g_target_health && (type == kTypePlayer || type == kTypeNPC))
      std::snprintf(buf, sizeof(buf), "%s%.*s %d%%%s%s", pre, name_len, base, hp_percent(ent, type), post, rest);
    else
      std::snprintf(buf, sizeof(buf), "%s%.*s%s%s", pre, name_len, base, post, rest);
    return buf;
  }

  return (base != text) ? base : nullptr;  // Generated player name, or no change.
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
  *display_flag = 1;
  reinterpret_cast<int(__thiscall *)(void *, int)>(kSetNameSpriteState)(entity, show);
}

// Per-frame (from the tint hook) check: if this entity is our target or self and the thing we
// modify changed (target identity, target HP%, or a hideself toggle), force a text rebuild.
// POD-bodied for rcp_guard.
static void maybe_refresh_text(void *entity) {
  if (!entity) return;

  void *target = *kTarget;

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

  // Generated names: rebuild each player once (ring-buffered) so a shownames change is
  // visible immediately, without waiting for the client to naturally re-set each player's name.
  if (*reinterpret_cast<uint8_t *>(static_cast<char *>(entity) + kEntType) == kTypePlayer) {
    void *actor = *reinterpret_cast<void **>(static_cast<char *>(entity) + kEntActor);
    if (actor && !player_recently_refreshed(actor)) {
      mark_player_refreshed(actor);
      force_state(entity);
    }
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
    const char *m = transform_entity(actor, text);
    if (m) {
      out = const_cast<char *>(m);
      // The two string params are NOT redundant: the client's clear path passes (NULL, "")
      // and its set path passes (name, copyOfName). param2 acts as presence/identity (empty ->
      // remove sprite, which is why hideself worked via param2 alone) while param3 is the text
      // the renderer draws - so the transform must be applied to BOTH or markers never show.
      static char copy[320];
      std::snprintf(copy, sizeof(copy), "%s", m);
      out_scratch = copy;
    }
  });
  return g_orig_setstr(actor, edx, flag, out, out_scratch);
}

// SetNameSpriteState detour. We only use it to record which entity's nameplate is being built,
// so the set-string hook (which sees just the actor) can resolve the full entity. Save/restore
// g_current_entity so nested rebuilds unwind correctly. __thiscall(this, bool show).
typedef int(__fastcall *StateFn)(void *self, int edx, int show);
static StateFn g_orig_state = nullptr;

static int __fastcall SetNameSpriteState_hk(void *self, int edx, int show) {
  void *prev = g_current_entity;
  g_current_entity = self;
  const int r = g_orig_state(self, edx, show);
  g_current_entity = prev;
  return r;
}

// Lazily detours the actor's set-string method the first time we have a live actor (its address
// lives in eqgfx and isn't known until an actor exists). Always installs: name generation is
// always active, so the text seam is always needed.
static void ensure_setstr_hook(void *entity) {
  if (g_setstr_installed || !entity) return;
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
  char msg[320];
  std::snprintf(msg, sizeof(msg),
                "rof2ClientPlus nameplates: concolors=%s | colors=%s | targetcolor=%s | targetblink=%s "
                "(%dms) | targetmarker=%s | targethealth=%s | hideself=%s",
                g_con_colors ? "on" : "off", g_state_colors ? "on" : "off", g_target_color ? "on" : "off",
                g_target_blink ? "on" : "off", g_blink_ms, g_target_marker ? "on" : "off",
                g_target_health ? "on" : "off", g_hide_self ? "on" : "off");
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

  // SetNameSpriteState detour: records the entity being drawn so the (actor-only) set-string hook
  // can resolve it. Cheap and side-effect-free, so install unconditionally.
  rcp->hooks->Add("nameplate_state", static_cast<int>(kSetNameSpriteState), SetNameSpriteState_hk, hook_type_detour);
  g_orig_state = rcp->hooks->hook_map["nameplate_state"]->original(SetNameSpriteState_hk);

  // The text-setter detour (N2/N3) installs lazily from the tint hook once a live actor exists
  // (its address is in eqgfx). See ensure_setstr_hook. Nothing to install here.
  logger::logf("[nameplate] text mods (targetmarker=%d targethealth=%d hideself=%d); setter installs in-game",
               (int)g_target_marker, (int)g_target_health, (int)g_hide_self);

  // Name generation is always active, so always refresh nameplate text on first sight.
  mark_text_dirty();

  // Intercept /shownames to capture the level (0-7) for our generator, then let the client's own
  // /shownames run (it rebuilds all nameplates, which re-runs our generation at the new level).
  rcp->commands_hook->Add("/shownames", {"/showname", "/show"}, "Show names (extended levels 5-7 supported).",
                          [](std::vector<std::string> &args) {
                            if (args.size() >= 2) {
                              if (args[1].rfind("off", 0) == 0)
                                g_shownames_level = 0;
                              else {
                                int v = std::atoi(args[1].c_str());
                                if (v >= 1 && v <= 7) g_shownames_level = v;
                              }
                              clear_player_ring();  // Re-generate every player at the new level.
                            }
                            return false;  // Let the client's own /shownames run too.
                          });

  rcp->commands_hook->Add(
      "/rcpnameplate", {"/rcpname"},
      "Nameplate options. '/rcpnameplate <concolors|colors|targetcolor|targetblink|targetmarker|targethealth|"
      "hideself> [on|off]' or '/rcpnameplate blinkspeed <200-3000ms>'.",
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
          else if (opt == "blinkspeed" && args.size() >= 3) {
            nameplate_settings::set_blink_ms(std::atoi(args[2].c_str()));
          } else {
            Rcp::Game::print_chat(
                "Usage: /rcpnameplate <concolors|colors|targetcolor|targetblink|targetmarker|targethealth|hideself> "
                "[on|off] | blinkspeed <200-3000>");
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
