// rof2ClientPlus - floating combat damage. See floating_damage.h.
#include "floating_damage.h"
#include "rebase.h"

#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "bitmap_font.h"  // SpriteFont (3-D billboard text; shared arial_bold_24 atlas).
#include "commands.h"
#include "crash_handler.h"
#include "font_overlay.h"     // add_scene_draw (shared post-world / pre-UI in-scene seam).
#include "game_functions.h"   // Rcp::Game::get_entity_by_id / is_in_game / print_chat.
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"
#include "vectors.h"

namespace {

constexpr char kIniSection[] = "FloatingDamage";

// ---- the damage source: CEverQuest::ReportSuccessfulHit ------------------------------------
// eqlib (offsets/eqgame.h, __ClientDate 20130510 == our build): CEverQuest__ReportSuccessfulHit
// = 0x52EE40, `bool ReportSuccessfulHit(EQSuccessfulHit*, bool, int)` - a __thiscall member. This
// is the function that prints the "You/X hit Y for N points" combat lines; hooking it (like Zeal
// hooks its TAKP equivalent) hands us every melee/spell damage event the client is told about.
const uintptr_t kReportSuccessfulHit = ::Rcp::eqva(0x52EE40);

// eqlib EQSuccessfulHit (RoF2 layout - NOT the TAKP Damage_Struct in game_packets.h, whose field
// sizes differ: here Skill is 1 byte and SpellID/DamageCaused are 4).
#pragma pack(push, 1)
struct EQSuccessfulHit {
  /*0x00*/ uint16_t DamagedID;    // spawn that was hit
  /*0x02*/ uint16_t AttackerID;   // spawn that did the hit
  /*0x04*/ uint8_t Skill;         // attack skill (1 = HS, etc.)
  /*0x05*/ int SpellID;           // > 0 for spell / non-melee damage
  /*0x09*/ int DamageCaused;      // <= 0 for a miss
  /*0x0d*/ float Force;
  /*0x11*/ float HitHeading;
  /*0x15*/ float HitPitch;
  /*0x19*/ bool bSecondary;
  /*0x1a*/ uint8_t Unknown0x1A[6];
};
#pragma pack(pop)

typedef bool(__fastcall *ReportHitFn)(void *pThis, int edx, EQSuccessfulHit *hit, bool output_text, int a3);
ReportHitFn g_orig = nullptr;

// ---- stock RoF2 globals + entity offsets (disasm-confirmed; see font_overlay.cpp / target_ring.cpp).
void **const kSelf = reinterpret_cast<void **>(::Rcp::eqva(0xDD2630));        // local player entity
void **const kControlled = reinterpret_cast<void **>(::Rcp::eqva(0xDD2644));  // controlled entity (self or a charm/merc)
constexpr int kEntPos0 = 0x64;           // renderer position floats, memory order (EQ Y).
constexpr int kEntPos1 = 0x68;           // (EQ X)
constexpr int kEntPos2 = 0x6c;           // vertical (EQ Z)
constexpr int kEntActor = 0x101c;        // CActor*; NULL => not in-world / no graphics yet.
constexpr int kActorStringSprite = 0x204;  // CStringSprite* (name sprite); NULL until a name is set.
constexpr int kSsWorldPos = 0x6c;          // head-anchor world pos on the sprite (0x6c, 0x70, 0x74).
constexpr int kEntAvatarHeight = 0x138;    // head height above the feet base (head-anchor fallback).
// eqlib PlayerBase (same struct the confirmed offsets above come from): SpawnID @0x148, MasterID
// @0x38c (a pet's owner spawn id). Only read under the crash guard, and only to classify who dealt
// the hit; a wrong value just mis-colors, never crashes.
constexpr int kEntSpawnId = 0x148;   // uint32 spawn id
constexpr int kEntMasterId = 0x38c;  // uint32 pet-owner spawn id (0 if not a pet)

// Spawn-id -> entity lookup. This is the EXACT path the client uses inside ReportSuccessfulHit
// itself (disasm: mov ecx,[0xE641D0]; push id; call 0x5996E0) and eqlib confirms it:
// PlayerManagerClient::GetSpawnByID(int) @0x5996E0, this = pinstSpawnManager @0xE641D0. The vendored
// Rcp::Game::get_entity_by_id (flat EntityIdArray@0x0078c47c) is a STALE TAKP address that returns a
// garbage pointer on RoF2 - do not use it.
void **const kSpawnManager = reinterpret_cast<void **>(::Rcp::eqva(0xE641D0));
const uintptr_t kGetSpawnByID = ::Rcp::eqva(0x5996E0);
void *get_spawn_by_id(int id) {
  void *mgr = *kSpawnManager;
  if (!mgr) return nullptr;
  return reinterpret_cast<void *(__thiscall *)(void *, int)>(kGetSpawnByID)(mgr, id);
}

// ---- live settings (persisted to rof2ClientPlus.ini [FloatingDamage]). Off by default. ----
bool g_enabled = false;
bool g_show_mine = true;
bool g_show_incoming = true;
bool g_show_others = true;
bool g_show_melee = true;
bool g_show_spells = true;
int g_big_hit = 100;
int g_color_mine = 0xFFF040;      // my damage: bright yellow
int g_color_incoming = 0xFF4040;  // damage to me: red
int g_color_other = 0xC8C8C8;     // everyone else: light grey
int g_color_crit = 0xFF9000;      // big hit: orange

constexpr int kBigHitCap = 5000;  // upper bound for the threshold (command + slider).

// ---- animation tuning (world units / milliseconds). Easy to tweak after an in-game look. ----
constexpr float kScaleNormal = 0.032f;  // SpriteFont world-space scale for arial_bold_24 (nameplate-ish).
constexpr float kScaleBig = 0.050f;     // big hits render larger.
constexpr float kBaseLift = 1.0f;       // world-Z lift above the head anchor at spawn (clears the nameplate).
constexpr float kRiseWorld = 5.0f;      // additional world-Z the number rises over its life.
constexpr float kJitter = 1.5f;         // per-number horizontal scatter (world units) so stacked hits spread.
constexpr unsigned kDurNormal = 2200;   // lifetime, ms.
constexpr unsigned kDurBig = 3000;
constexpr size_t kMaxFloaters = 64;     // bound the active set (drop the oldest past this).

// One active floating number. spawn_id is re-resolved to a live entity each frame (the entity may
// despawn); text/color/scale are fixed at spawn; the rise + fade are derived from age.
struct Floater {
  uint16_t spawn_id;
  std::string text;
  int color_rgb;
  bool big;
  unsigned long long start_tick;
  unsigned duration_ms;
  float jitter_x;
  float jitter_y;
};

std::mutex g_mutex;             // guards g_floaters (hook fires on the main thread, draw on the render thread).
std::vector<Floater> g_floaters;

// Render-thread-owned billboard font (created lazily against the live device, like the nameplates).
std::unique_ptr<SpriteFont> g_font;

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Small non-critical jitter for scatter; deterministic sequence is fine (visual only).
float frand(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(std::rand()) / RAND_MAX); }

int parse_hex_color(const std::string &s) {
  const char *p = s.c_str();
  if (*p == '#') ++p;
  return static_cast<int>(std::strtoul(p, nullptr, 16) & 0xffffff);
}

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "Enabled")) g_enabled = ini.getValue<bool>(kIniSection, "Enabled");
  if (ini.exists(kIniSection, "ShowMine")) g_show_mine = ini.getValue<bool>(kIniSection, "ShowMine");
  if (ini.exists(kIniSection, "ShowIncoming")) g_show_incoming = ini.getValue<bool>(kIniSection, "ShowIncoming");
  if (ini.exists(kIniSection, "ShowOthers")) g_show_others = ini.getValue<bool>(kIniSection, "ShowOthers");
  if (ini.exists(kIniSection, "ShowMelee")) g_show_melee = ini.getValue<bool>(kIniSection, "ShowMelee");
  if (ini.exists(kIniSection, "ShowSpells")) g_show_spells = ini.getValue<bool>(kIniSection, "ShowSpells");
  if (ini.exists(kIniSection, "BigHit")) g_big_hit = ini.getValue<int>(kIniSection, "BigHit");
  if (ini.exists(kIniSection, "ColorMine")) g_color_mine = parse_hex_color(ini.getValue<std::string>(kIniSection, "ColorMine"));
  if (ini.exists(kIniSection, "ColorIncoming"))
    g_color_incoming = parse_hex_color(ini.getValue<std::string>(kIniSection, "ColorIncoming"));
  if (ini.exists(kIniSection, "ColorOther")) g_color_other = parse_hex_color(ini.getValue<std::string>(kIniSection, "ColorOther"));
  if (ini.exists(kIniSection, "ColorCrit")) g_color_crit = parse_hex_color(ini.getValue<std::string>(kIniSection, "ColorCrit"));
  if (g_big_hit < 0) g_big_hit = 0;
  if (g_big_hit > kBigHitCap) g_big_hit = kBigHitCap;
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "Enabled", g_enabled);
  ini.setValue<bool>(kIniSection, "ShowMine", g_show_mine);
  ini.setValue<bool>(kIniSection, "ShowIncoming", g_show_incoming);
  ini.setValue<bool>(kIniSection, "ShowOthers", g_show_others);
  ini.setValue<bool>(kIniSection, "ShowMelee", g_show_melee);
  ini.setValue<bool>(kIniSection, "ShowSpells", g_show_spells);
  ini.setValue<int>(kIniSection, "BigHit", g_big_hit);
  char hex[8];
  std::snprintf(hex, sizeof(hex), "%06X", g_color_mine & 0xffffff);
  ini.setValue<std::string>(kIniSection, "ColorMine", hex);
  std::snprintf(hex, sizeof(hex), "%06X", g_color_incoming & 0xffffff);
  ini.setValue<std::string>(kIniSection, "ColorIncoming", hex);
  std::snprintf(hex, sizeof(hex), "%06X", g_color_other & 0xffffff);
  ini.setValue<std::string>(kIniSection, "ColorOther", hex);
  std::snprintf(hex, sizeof(hex), "%06X", g_color_crit & 0xffffff);
  ini.setValue<std::string>(kIniSection, "ColorCrit", hex);
}

// Classify one hit into plain primitives. Runs inside the crash guard (raw entity-memory reads);
// it holds NO lock and builds NO non-trivial objects, so a fault here just abandons the read (a
// longjmp can't strand a mutex or skip a destructor that matters).
struct HitInfo {
  bool valid = false;
  int dmg = 0;
  bool is_spell = false;
  uint16_t target_id = 0;
  bool is_mine = false;
  bool is_to_me = false;
};

void classify(const EQSuccessfulHit *hit, HitInfo &out) {
  out.dmg = hit->DamageCaused;
  out.is_spell = hit->SpellID > 0;
  out.target_id = hit->DamagedID;
  void *self = *kSelf;
  void *controlled = *kControlled;
  void *target = get_spawn_by_id(hit->DamagedID);
  void *source = get_spawn_by_id(hit->AttackerID);
  if (!target) return;  // No entity to anchor the number to.
  out.is_to_me = (target == self || target == controlled);
  if (source) {
    if (source == self) {
      out.is_mine = true;
    } else if (self) {
      uint32_t self_id = *reinterpret_cast<uint32_t *>(static_cast<char *>(self) + kEntSpawnId);
      uint32_t src_master = *reinterpret_cast<uint32_t *>(static_cast<char *>(source) + kEntMasterId);
      if (src_master != 0 && src_master == self_id) out.is_mine = true;  // my pet
    }
  }
  out.valid = true;
}

// Detour: capture the hit, then always forward to the original (so the client still prints/plays
// the combat line exactly as before). Gated on enabled/in-game/not-shutting-down.
bool __fastcall report_hit_hk(void *pThis, int edx, EQSuccessfulHit *hit, bool output_text, int a3) {
  if (g_enabled && hit && !crash_handler::shutting_down() && Rcp::Game::is_in_game()) {
    HitInfo info;
    rcp_guard::run("fcd.classify", [&] { classify(hit, info); });
    // Filtering + string build + push happen OUTSIDE the guard (plain globals + heap only).
    if (info.valid && info.dmg > 0) {
      bool pass = info.is_spell ? g_show_spells : g_show_melee;
      if (pass) {
        if (info.is_mine)
          pass = g_show_mine;
        else if (info.is_to_me)
          pass = g_show_incoming;
        else
          pass = g_show_others;
      }
      if (pass) {
        const bool big = info.dmg >= g_big_hit;
        Floater f;
        f.spawn_id = info.target_id;
        f.text = std::to_string(info.dmg);
        f.color_rgb = (big ? g_color_crit : info.is_mine ? g_color_mine : info.is_to_me ? g_color_incoming : g_color_other) & 0xffffff;
        f.big = big;
        f.start_tick = GetTickCount64();
        f.duration_ms = big ? kDurBig : kDurNormal;
        f.jitter_x = frand(-kJitter, kJitter);
        f.jitter_y = frand(-kJitter, kJitter);
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_floaters.size() >= kMaxFloaters) g_floaters.erase(g_floaters.begin());
        g_floaters.push_back(std::move(f));
      }
    }
  }
  return g_orig(pThis, edx, hit, output_text, a3);
}

// Queue one floater as a billboard at its entity's head (rising + fading). Returns false if the
// entity can't be resolved this frame (despawned / out of view of the id array). Reads raw entity
// memory; the whole draw runs inside directx's rcp_guard (registered via add_scene_draw).
bool queue_floater(const Floater &f, unsigned long long now) {
  void *ent = get_spawn_by_id(f.spawn_id);
  if (!ent) return false;
  char *e = static_cast<char *>(ent);
  void *actor = *reinterpret_cast<void **>(e + kEntActor);
  if (!actor) return false;  // Not in-world / no graphics.

  const float p0 = *reinterpret_cast<float *>(e + kEntPos0);
  const float p1 = *reinterpret_cast<float *>(e + kEntPos1);
  const float p2 = *reinterpret_cast<float *>(e + kEntPos2);
  // Head anchor: the model's name-sprite world pos (native's own head anchor, scales per model),
  // falling back to feet + AvatarHeight if the sprite isn't up yet - identical to the nameplates.
  float h0, h1, h2;
  void *ss = *reinterpret_cast<void **>(static_cast<char *>(actor) + kActorStringSprite);
  if (ss) {
    h0 = *reinterpret_cast<float *>(static_cast<char *>(ss) + kSsWorldPos + 0);
    h1 = *reinterpret_cast<float *>(static_cast<char *>(ss) + kSsWorldPos + 4);
    h2 = *reinterpret_cast<float *>(static_cast<char *>(ss) + kSsWorldPos + 8);
  } else {
    h0 = p0;
    h1 = p1;
    h2 = p2 + *reinterpret_cast<float *>(e + kEntAvatarHeight);
  }

  const float t = clampf(static_cast<float>(now - f.start_tick) / static_cast<float>(f.duration_ms), 0.0f, 1.0f);
  const float rise = kBaseLift + t * kRiseWorld;             // rise in world-Z above the head
  const float alpha = t < 0.5f ? 1.0f : (1.0f - (t - 0.5f) / 0.5f);  // hold, then fade the back half
  const BYTE a = static_cast<BYTE>(clampf(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
  const D3DCOLOR color = (static_cast<D3DCOLOR>(a) << 24) | (f.color_rgb & 0xffffff);

  Vec3 pos(h0 + f.jitter_x, h1 + f.jitter_y, h2 + rise);
  g_font->queue_string(f.text.c_str(), pos, /*center=*/true, color);
  return true;
}

// Render callback at the shared post-world / pre-UI seam (already rcp_guard-wrapped).
void draw(IDirect3DDevice9 *device) {
  if (!g_enabled || !device) {
    if (!g_enabled) g_font.reset();
    return;
  }
  if (!Rcp::Game::is_in_game()) return;

  const unsigned long long now = GetTickCount64();
  // Snapshot + expire under the lock (short), then render without holding it (no lock across the
  // billboard draws / raw reads, so a guard longjmp can never strand the mutex).
  std::vector<Floater> snapshot;
  {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_floaters.empty()) return;
    g_floaters.erase(std::remove_if(g_floaters.begin(), g_floaters.end(),
                                    [now](const Floater &f) { return (now - f.start_tick) >= f.duration_ms; }),
                     g_floaters.end());
    if (g_floaters.empty()) return;
    snapshot = g_floaters;
  }

  if (!g_font) {
    g_font = SpriteFont::create_sprite_font(*device, "arial_bold_24");
    if (!g_font) {
      logger::log("[fcd] SpriteFont create failed; disabling");
      g_enabled = false;
      return;
    }
    g_font->set_drop_shadow(true);
  }

  // Two flushes: scale is a per-flush font setting, so bucket normal vs big hits. Everything else
  // (position, color, alpha) is per-queued-string.
  for (int pass = 0; pass < 2; ++pass) {
    const bool big_pass = (pass == 1);
    g_font->set_scale_factor(big_pass ? kScaleBig : kScaleNormal);
    bool any = false;
    for (const Floater &f : snapshot) {
      if (f.big != big_pass) continue;
      if (queue_floater(f, now)) any = true;
    }
    if (any) g_font->flush_queue_to_screen();
  }
}

void print_status() {
  Rcp::Game::print_chat(
      "rof2ClientPlus floating combat damage: %s | mine %s, to-me %s, others %s | melee %s, spells %s | big-hit %d",
      g_enabled ? "ON" : "OFF", g_show_mine ? "on" : "off", g_show_incoming ? "on" : "off",
      g_show_others ? "on" : "off", g_show_melee ? "on" : "off", g_show_spells ? "on" : "off", g_big_hit);
}

bool as_on(const std::string &s) { return s == "on" || s == "1" || s == "true"; }

}  // namespace

// ---- options-UI / command accessors (apply live + persist) ----
namespace floating_damage_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  save_settings();
}
bool get_show_mine() { return g_show_mine; }
void set_show_mine(bool on) {
  g_show_mine = on;
  save_settings();
}
bool get_show_incoming() { return g_show_incoming; }
void set_show_incoming(bool on) {
  g_show_incoming = on;
  save_settings();
}
bool get_show_others() { return g_show_others; }
void set_show_others(bool on) {
  g_show_others = on;
  save_settings();
}
bool get_show_melee() { return g_show_melee; }
void set_show_melee(bool on) {
  g_show_melee = on;
  save_settings();
}
bool get_show_spells() { return g_show_spells; }
void set_show_spells(bool on) {
  g_show_spells = on;
  save_settings();
}
int get_big_hit() { return g_big_hit; }
void set_big_hit(int threshold) {
  g_big_hit = threshold < 0 ? 0 : (threshold > kBigHitCap ? kBigHitCap : threshold);
  save_settings();
}
int big_hit_cap() { return kBigHitCap; }
int get_color_mine() { return g_color_mine & 0xffffff; }
void set_color_mine(int rgb) {
  g_color_mine = rgb & 0xffffff;
  save_settings();
}
int get_color_incoming() { return g_color_incoming & 0xffffff; }
void set_color_incoming(int rgb) {
  g_color_incoming = rgb & 0xffffff;
  save_settings();
}
int get_color_other() { return g_color_other & 0xffffff; }
void set_color_other(int rgb) {
  g_color_other = rgb & 0xffffff;
  save_settings();
}
int get_color_crit() { return g_color_crit & 0xffffff; }
void set_color_crit(int rgb) {
  g_color_crit = rgb & 0xffffff;
  save_settings();
}
}  // namespace floating_damage_settings

FloatingDamage::FloatingDamage(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  logger::logf("[fcd] settings loaded: enabled=%d mine=%d incoming=%d others=%d melee=%d spells=%d bighit=%d",
               (int)g_enabled, (int)g_show_mine, (int)g_show_incoming, (int)g_show_others, (int)g_show_melee,
               (int)g_show_spells, g_big_hit);

  // Detour the client's combat-damage report (installs now; the body no-ops until enabled).
  rcp->hooks->Add("floating_damage", static_cast<int>(kReportSuccessfulHit), report_hit_hk, hook_type_detour);
  g_orig = rcp->hooks->hook_map["floating_damage"]->original(report_hit_hk);
  logger::logf("[fcd] ReportSuccessfulHit detour installed at 0x%x", (unsigned)kReportSuccessfulHit);

  // Draw at the shared post-world / pre-UI seam (installs whenever FontOverlay exists).
  font_overlay::add_scene_draw([](IDirect3DDevice9 *dev) { draw(dev); });

  rcp->commands_hook->Add(
      "/rcpfcd", {"/rcpdamage"},
      "Floating combat damage (Zeal-style): '/rcpfcd' toggles; 'on|off', 'mine on|off', 'incoming on|off', "
      "'others on|off', 'melee on|off', 'spells on|off', 'bighit <n>', "
      "'color mine|incoming|others|big RRGGBB'.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1") {
            floating_damage_settings::set_enabled(true);
          } else if (a == "off" || a == "0") {
            floating_damage_settings::set_enabled(false);
          } else if (a == "mine" && args.size() >= 3) {
            floating_damage_settings::set_show_mine(as_on(args[2]));
          } else if (a == "incoming" && args.size() >= 3) {
            floating_damage_settings::set_show_incoming(as_on(args[2]));
          } else if (a == "others" && args.size() >= 3) {
            floating_damage_settings::set_show_others(as_on(args[2]));
          } else if (a == "melee" && args.size() >= 3) {
            floating_damage_settings::set_show_melee(as_on(args[2]));
          } else if (a == "spells" && args.size() >= 3) {
            floating_damage_settings::set_show_spells(as_on(args[2]));
          } else if (a == "bighit" && args.size() >= 3) {
            floating_damage_settings::set_big_hit(std::atoi(args[2].c_str()));
          } else if (a == "color" && args.size() >= 4) {
            const std::string &which = args[2];
            const int rgb = parse_hex_color(args[3]);
            if (which == "mine")
              floating_damage_settings::set_color_mine(rgb);
            else if (which == "incoming")
              floating_damage_settings::set_color_incoming(rgb);
            else if (which == "others" || which == "other")
              floating_damage_settings::set_color_other(rgb);
            else if (which == "big" || which == "crit")
              floating_damage_settings::set_color_crit(rgb);
            else
              Rcp::Game::print_chat("rof2ClientPlus: color target is 'mine', 'incoming', 'others', or 'big'.");
          } else {
            Rcp::Game::print_chat(
                "rof2ClientPlus: '/rcpfcd on|off | mine on|off | incoming on|off | others on|off | "
                "melee on|off | spells on|off | bighit <n> | color mine|incoming|others|big RRGGBB'");
            return true;
          }
        } else {
          floating_damage_settings::set_enabled(!floating_damage_settings::get_enabled());
        }
        print_status();
        return true;
      });
  logger::log("[fcd] FloatingDamage constructed; /rcpfcd registered");
}
