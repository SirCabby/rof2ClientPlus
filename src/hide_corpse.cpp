// rof2ClientPlus - continuous corpse hiding. See hide_corpse.h.
#include "hide_corpse.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "directx.h"
#include "game_functions.h"  // Rcp::Game::print_chat / is_in_game
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"
#include "string_util.h"  // Rcp::String::compare_insensitive

namespace {

// ---- Confirmed RoF2 eqlib PlayerClient (spawn) layout, shared with the in-game
// verified modules (nameplate.cpp / font_overlay.cpp / floating_damage.cpp). ----
void **const kSpawnManager = reinterpret_cast<void **>(0xE641D0);  // pinstSpawnManager (PlayerManagerClient).
constexpr int kListFirst = 0x08;    // manager+0x08 -> first spawn in the linked list.
constexpr int kEntNext = 0x08;      // spawn+0x08   -> next spawn.
constexpr int kEntType = 0x125;     // uint8: 0=Player, 1=NPC, 2=Corpse (npc AND player corpses collapse to 2).
constexpr int kEntActor = 0x101c;   // CActorInterface*; NULL => no graphics actor yet (not in-world).
constexpr int kEntSpawnId = 0x148;  // uint32 spawn id.
constexpr uint8_t kTypeCorpse = 2;

// PlayerManagerClient::GetSpawnByID(int) @0x5996E0, this = *kSpawnManager. The exact path the client
// uses; the vendored Rcp::Game::get_entity_by_id is a stale TAKP address (garbage on RoF2) - never use it.
constexpr uintptr_t kGetSpawnByID = 0x5996E0;
void *get_spawn_by_id(int id) {
  void *mgr = *kSpawnManager;
  if (!mgr) return nullptr;
  return reinterpret_cast<void *(__thiscall *)(void *, int)>(kGetSpawnByID)(mgr, id);
}

constexpr int kEntName = 0xe4;      // displayed name (pre-trimmed); a corpse shows "<Base>'s corpse".
constexpr int kEntCandDiscrim = 0x150;  // candidate PC/NPC discriminator to log (fork RE; getter 0x8CFCE0).
void **const kTarget = reinterpret_cast<void **>(0xDD2648);  // current target spawn (for /hidecorpse debug).

inline uint8_t ent_type(char *ent) { return *reinterpret_cast<uint8_t *>(ent + kEntType); }
inline void *ent_actor(char *ent) { return *reinterpret_cast<void **>(ent + kEntActor); }
inline uint32_t ent_spawnid(char *ent) { return *reinterpret_cast<uint32_t *>(ent + kEntSpawnId); }

// Safely invoke a __thiscall method `actor->vtable[slot](arg)` when we don't know the callee's exact
// arg count: we save ESP before the call and restore it after, so a callee that does `ret 0`/`ret 8`
// (instead of the `ret 4` we assumed) cannot corrupt the stack. Used to probe the CActorInterface
// vtable for the model-visibility method on a single targeted corpse (the /hidecorpses call command).
int call_actor_vtable_1(void *actor, int slot, int arg) {
  void **vt = *reinterpret_cast<void ***>(actor);
  void *fn = vt[slot];
  int result;
  __asm__ __volatile__(
      "movl %%esp, %%ebx\n\t"       // save esp
      "pushl %[arg]\n\t"            // push the single arg (works for a bool/int setter)
      "movl %[thisp], %%ecx\n\t"    // ecx = this (thiscall)
      "call *%[fn]\n\t"
      "movl %%ebx, %%esp\n\t"       // restore esp (undo any ret-N stack imbalance)
      : "=a"(result)
      : [fn] "r"(fn), [arg] "r"(arg), [thisp] "r"(actor)
      : "ebx", "ecx", "edx", "cc", "memory");
  return result;
}

// ---- RoF2 model-hide primitive (fork RE, disassembly-confirmed; see PORTING_NOTES). ----
// A corpse's 3D model is drawn only while its render actor (a CHierarchicalActor at spawn+0x101c) is
// registered in EQGraphicsDX9.dll's scene manager. HIDE = remove the actor from the scene - the exact
// call the actor's OWN destructor makes: manager->vtable[13](actor) (slot +0x34, unlinks the actor from
// all four spatial buckets). SHOW = re-add it: manager->vtable[12](actor) (slot +0x30). The SPAWN stays
// in the world - it's still targetable and lootable; only the model stops drawing (which is what lets a
// hidden corpse still be looted, and what showlast reveals). The scene manager is a DLL global at
// RVA 0x179110; we resolve the DLL base at runtime (GetModuleHandleA) so it's ASLR-safe.
//
// IMPORTANT: remove/add are NOT idempotent flags - each must be called exactly ONCE per corpse per
// state change (a double-add would link the actor into a bucket twice). The scan below removes a corpse
// once when first seen (tracked in g_hidden); showlast/toggle-off add back once. Remove searches the
// buckets, so a stray double-remove (e.g. the client's own dtor after we removed) is a harmless no-op.
// The reversible per-corpse visibility flag: a byte on the render actor at +0xd1. 1 = hidden (the engine
// skips drawing the model), 0 = visible. CONFIRMED in-game: setting it 1 hides the corpse, 0 restores it.
// It is exactly the field behind the actor's vtable slot 3 (a plain `mov [ecx+0xd1], al; ret 4`), so a
// direct byte write is identical to the client's own setter and cannot crash. Non-destructive: the spawn
// and its actor stay fully intact (targetable, lootable) - only rendering is gated - which is what makes
// this cleanly reversible (unlike scene removal, which is one-way).
constexpr int kActorHiddenFlag = 0xd1;      // actor byte: 1 = model hidden, 0 = visible.
constexpr int kEntInvisState = 0x338;       // (debug only) the /hideme name-sprite field, logged by debug.
inline int *invis_field(char *ent) { return reinterpret_cast<int *>(ent + kEntInvisState); }

void hide_actor(void *actor) {
  if (actor) *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(actor) + kActorHiddenFlag) = 1;
}
void show_actor(void *actor) {
  if (actor) *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(actor) + kActorHiddenFlag) = 0;
}

void hide_model(char *ent) { hide_actor(ent_actor(ent)); }
void show_model(char *ent) { show_actor(ent_actor(ent)); }

void **const kSelf = reinterpret_cast<void **>(0xDD2630);  // local player spawn.

// A corpse's displayed name (+0xe4) is "<Base>'s corpse"; copy out <Base> (text before the apostrophe,
// trailing space trimmed).
void corpse_base_name(char *ent, char *out, size_t outsz) {
  const char *name = ent + kEntName;
  size_t n = 0;
  for (const char *c = name; *c && *c != '\'' && n < outsz - 1; ++c) out[n++] = *c;
  while (n > 0 && out[n - 1] == ' ') --n;  // trim a trailing space before the possessive.
  out[n] = '\0';
}

// EQ player names are always a single capitalized alphabetic token (no space, digit, underscore, or
// apostrophe). Any corpse whose base name has the player-name shape is treated as a PLAYER corpse. NPC
// corpses ("a bat", "an orc pawn", "Lord Nagafen", "gnoll_pup039") fail the shape. Trade-off: a
// single-token capitalized *named* NPC (e.g. "Nagafen") is conservatively classed as a player corpse.
bool looks_like_player_name(const char *base) {
  if (!base || !base[0]) return false;
  if (!(base[0] >= 'A' && base[0] <= 'Z')) return false;  // NPC display names start lowercase ("a bat").
  for (const char *c = base; *c; ++c) {
    const char ch = *c;
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) return false;  // space/digit/_/'/other => not a player.
  }
  return true;
}

bool is_npc_corpse(char *ent) {
  if (ent_type(ent) != kTypeCorpse) return false;
  char base[64];
  corpse_base_name(ent, base, sizeof(base));
  return !looks_like_player_name(base);  // player-shaped => keep visible; everything else => NPC corpse.
}

// Case-insensitive full-string compare.
bool name_ieq(const char *a, const char *b) {
  for (size_t i = 0;; ++i) {
    char ca = a[i], cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb) return false;
    if (!ca) return true;
  }
}

// True if this corpse is the local player's OWN (base name == self's name).
bool is_own_corpse(char *ent) {
  void *self = *kSelf;
  if (!self) return false;
  char base[64];
  corpse_base_name(ent, base, sizeof(base));
  return base[0] && name_ieq(base, static_cast<char *>(self) + kEntName);
}

// True if this corpse belongs to a current group member (so 'allbutgroup' keeps it visible for a rez).
// Group offsets reused from the confirmed nameplate code: pLocalPC (*0xDD261C) -> CGroup* @ +0x31cc ->
// member ptr[6] @ +0x04, each member's PlayerClient* @ +0x28 (its name @ +0xe4). We match by NAME because
// a corpse is a different spawn than the live member.
constexpr uintptr_t kLocalPcPtr = 0xDD261C;
constexpr int kPcGroup = 0x31cc;
constexpr int kGroupMembers = 0x04;
constexpr int kGroupMemberPlayer = 0x28;
constexpr int kGroupMaxMembers = 6;
bool is_group_member_corpse(char *ent) {
  char base[64];
  corpse_base_name(ent, base, sizeof(base));
  if (!base[0]) return false;
  void *pc = *reinterpret_cast<void **>(kLocalPcPtr);
  if (!pc) return false;
  void *group = *reinterpret_cast<void **>(static_cast<char *>(pc) + kPcGroup);
  if (!group) return false;
  for (int i = 0; i < kGroupMaxMembers; ++i) {
    void *member = *reinterpret_cast<void **>(static_cast<char *>(group) + kGroupMembers + i * 4);
    if (!member) continue;
    void *pl = *reinterpret_cast<void **>(static_cast<char *>(member) + kGroupMemberPlayer);
    if (pl && name_ieq(base, static_cast<char *>(pl) + kEntName)) return true;
  }
  return false;
}

enum Scope { ScopeNPC, ScopeAll, ScopeAllButGroup };

// The hide filter for a scope. Own corpse is never hidden. NPC = NPC-shaped only (leaves all player
// corpses visible). All = every other corpse. AllButGroup = every other corpse except current group
// members' (kept visible for rezzing).
bool should_hide(char *ent, Scope scope) {
  if (ent_type(ent) != kTypeCorpse) return false;
  if (is_own_corpse(ent)) return false;
  if (scope == ScopeNPC) return is_npc_corpse(ent);
  if (scope == ScopeAllButGroup && is_group_member_corpse(ent)) return false;
  return true;  // ScopeAll, or ScopeAllButGroup non-group corpse.
}

// Master kill-switch. The /hideme field write is safe, so writes ship armed; a future /hidecorpse arm
// could flip this if a build ever needs the scan inert.
bool g_writes_armed = true;

// Pick a corpse to inspect: the current target if it's a corpse, else the first corpse in the spawn list.
char *find_corpse_for_debug() {
  void *t = *kTarget;
  if (t && ent_type(static_cast<char *>(t)) == kTypeCorpse) return static_cast<char *>(t);
  void *mgr = *kSpawnManager;
  if (!mgr) return nullptr;
  for (void *e = *reinterpret_cast<void **>(static_cast<char *>(mgr) + kListFirst); e;
       e = *reinterpret_cast<void **>(static_cast<char *>(e) + kEntNext))
    if (ent_type(static_cast<char *>(e)) == kTypeCorpse) return static_cast<char *>(e);
  return nullptr;
}

// Read-only dump of a corpse's render actor (+0x101c) and its FULL vtable to rof2ClientPlus.log. The
// actor's model-visibility method lives in EQGraphicsDX9.dll and its vtable address is only knowable at
// runtime (it's read from the live object), so this one live dump is what lets us statically identify the
// SetVisible/Hide slot in the DLL. Purely read-only (reads the DLL's .rdata + the actor object).
void do_debug() {
  char *e = find_corpse_for_debug();
  if (!e) {
    Rcp::Game::print_chat("hidecorpses debug: no corpse found - target one (or be near one) and retry.");
    return;
  }
  void *actor = ent_actor(e);
  Rcp::Game::print_chat("hidecorpses debug: corpse '%s' spawnid=%u actor=%p -> full vtable dump written to "
                        "rof2ClientPlus.log",
                        e + kEntName, ent_spawnid(e), actor);
  logger::logf("[hcdbg] ===== corpse actor dump =====");
  logger::logf("[hcdbg] corpse=%p name='%s' type=%d spawnid=%u +0x150=%d +0x338=%d npc=%d", (void *)e, e + kEntName,
               (int)ent_type(e), ent_spawnid(e), *reinterpret_cast<int *>(e + kEntCandDiscrim), *invis_field(e),
               (int)is_npc_corpse(e));
  logger::logf("[hcdbg] exe_base=%p EQGraphicsDX9.dll_base=%p", (void *)GetModuleHandleA(NULL),
               (void *)GetModuleHandleA("EQGraphicsDX9.dll"));
  if (!actor) {
    logger::logf("[hcdbg] actor is NULL (model not in-world) - retry on a visible corpse");
    return;
  }
  // Actor object header (a direct visibility flag, if any, would live here).
  uint32_t *a = reinterpret_cast<uint32_t *>(actor);
  for (int i = 0; i < 24; i += 8)
    logger::logf("[hcdbg] actor+0x%02x: %08x %08x %08x %08x %08x %08x %08x %08x", i * 4, a[i], a[i + 1], a[i + 2],
                 a[i + 3], a[i + 4], a[i + 5], a[i + 6], a[i + 7]);
  // Full vtable (methods in the DLL are the T3D actor's; one of them is SetVisible/Hide).
  void **vt = *reinterpret_cast<void ***>(actor);
  logger::logf("[hcdbg] vtable=%p", (void *)vt);
  for (int i = 0; i < 160; i += 8)
    logger::logf("[hcdbg] vt[%3d]: %p %p %p %p %p %p %p %p", i, vt[i], vt[i + 1], vt[i + 2], vt[i + 3], vt[i + 4],
                 vt[i + 5], vt[i + 6], vt[i + 7]);
  logger::logf("[hcdbg] ===== end dump =====");
}

// Probe: call actor->vtable[slot](arg) on a single corpse (the target, else the first corpse). Used to
// empirically confirm the model-visibility method identified from the vtable RE, contained to one corpse
// so a wrong guess can't touch every corpse via the per-frame scan.
void do_call(int slot, int arg) {
  char *e = find_corpse_for_debug();
  if (!e) {
    Rcp::Game::print_chat("hidecorpses call: no corpse found - target one (or be near one).");
    return;
  }
  void *actor = ent_actor(e);
  if (!actor) {
    Rcp::Game::print_chat("hidecorpses call: corpse '%s' has no actor.", e + kEntName);
    return;
  }
  logger::logf("[hcdbg] call vtable[%d](%d) on actor=%p corpse='%s'", slot, arg, actor, e + kEntName);
  int r = call_actor_vtable_1(actor, slot, arg);
  Rcp::Game::print_chat("hidecorpses call: '%s' vtable[%d](%d) -> %d", e + kEntName, slot, arg, r);
  logger::logf("[hcdbg] call vtable[%d](%d) returned %d", slot, arg, r);
}

// Empirical float poker: set a float field on the target corpse's actor. Used to find the reversible
// SCALE field (zeroing it hides the model, restoring shows it) - just a float write, cannot crash.
void do_setf(int off, double val) {
  char *e = find_corpse_for_debug();
  if (!e) {
    Rcp::Game::print_chat("hidecorpses setf: no corpse found - target one (or be near one).");
    return;
  }
  void *actor = ent_actor(e);
  if (!actor) {
    Rcp::Game::print_chat("hidecorpses setf: corpse '%s' has no actor.", e + kEntName);
    return;
  }
  float *p = reinterpret_cast<float *>(reinterpret_cast<char *>(actor) + off);
  float old = *p;
  *p = static_cast<float>(val);
  Rcp::Game::print_chat("hidecorpses setf: '%s' actor+0x%x  %.3f -> %.3f", e + kEntName, off, old, *p);
  logger::logf("[hcdbg] setf actor+0x%x %.3f -> %.3f", off, (double)old, val);
}

// Single-corpse reversibility test. testhide removes the TARGET corpse's actor from the scene and
// remembers the actor pointer; testshow re-adds ALL remembered actors (no targeting needed, since a
// hidden corpse can't be clicked). This confirms whether scene remove/add is a reversible pair.
std::unordered_set<void *> g_test_removed;
void do_test(bool hide) {
  if (hide) {
    char *e = find_corpse_for_debug();
    if (!e) {
      Rcp::Game::print_chat("hidecorpses testhide: no corpse found - target one (or be near one).");
      return;
    }
    void *actor = ent_actor(e);
    if (!actor) {
      Rcp::Game::print_chat("hidecorpses testhide: corpse '%s' has no actor.", e + kEntName);
      return;
    }
    if (g_test_removed.count(actor)) {
      Rcp::Game::print_chat("hidecorpses testhide: '%s' already removed.", e + kEntName);
      return;
    }
    logger::logf("[hcdbg] testhide actor=%p corpse='%s'", actor, e + kEntName);
    hide_actor(actor);
    g_test_removed.insert(actor);
    Rcp::Game::print_chat("hidecorpses testhide: '%s' - should VANISH. Then /hidecorpses testshow to re-add it.",
                          e + kEntName);
  } else {
    if (g_test_removed.empty()) {
      Rcp::Game::print_chat("hidecorpses testshow: nothing was testhidden this session.");
      return;
    }
    int n = 0;
    for (void *actor : g_test_removed) {
      logger::logf("[hcdbg] testshow re-add actor=%p", actor);
      show_actor(actor);
      ++n;
    }
    g_test_removed.clear();
    Rcp::Game::print_chat("hidecorpses testshow: re-added %d corpse(s) - they should REAPPEAR.", n);
  }
}

// ---- live state ----
bool g_always = false;  // Persisted; off by default.
bool g_looted = false;  // Persisted; auto-hide each corpse after you finish looting it.

// Corpses we have hidden this session: spawn id -> the ent pointer we hid (ptr validates against
// spawn-id reuse). Used to detect newly-appeared corpses (for "last hidden") and to reveal them all
// when 'always' is switched off.
std::unordered_map<uint32_t, void *> g_hidden;
// Corpses the user explicitly revealed via showlast: exempt from re-hiding while they exist.
std::unordered_map<uint32_t, void *> g_revealed;
// The most-recently newly-hidden corpse (what showlast reveals). {nullptr,0} = none.
void *g_last_hidden_ent = nullptr;
uint32_t g_last_hidden_id = 0;

constexpr char kIniSection[] = "HideCorpse";
constexpr char kIniKeyAlways[] = "Always";
constexpr char kIniKeyLooted[] = "Looted";

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKeyAlways)) g_always = ini.getValue<bool>(kIniSection, kIniKeyAlways);
  if (ini.exists(kIniSection, kIniKeyLooted)) g_looted = ini.getValue<bool>(kIniSection, kIniKeyLooted);
}
void save_always() { IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, kIniKeyAlways, g_always); }
void save_looted() { IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, kIniKeyLooted, g_looted); }

// Reveal every corpse we hid this session (called when 'always' is switched off). Best-effort: a
// despawned corpse simply resolves to nullptr and is skipped.
void reveal_all_hidden() {
  for (const auto &[id, ent] : g_hidden) {
    void *live = get_spawn_by_id(static_cast<int>(id));
    if (live && live == ent) show_model(static_cast<char *>(live));
  }
  g_hidden.clear();
  g_revealed.clear();
  g_last_hidden_ent = nullptr;
  g_last_hidden_id = 0;
}

// 'looted' mode: detour CLootWnd::Deactivate (0x6bd0b0, __thiscall(this), ret) - the loot window's close
// handler (it clears the target at 0xDD2648 and resets loot state). On entry the target is still the
// corpse you were looting (you target a corpse to loot it), so we grab it and hide it. Tracked in
// g_hidden so showlast/off work with it too. The __fastcall(this, edx) shape is the standard __thiscall
// detour idiom used across this codebase (chat_timestamp / sound_mods).
constexpr uintptr_t kLootDeactivate = 0x6bd0b0;
typedef void(__fastcall *LootDeactivateFn)(void *lw, int edx);
LootDeactivateFn g_loot_orig = nullptr;

void __fastcall loot_deactivate_hk(void *lw, int edx) {
  if (g_looted && !crash_handler::shutting_down() && Rcp::Game::is_in_game()) {
    void *t = *kTarget;
    if (t && ent_type(static_cast<char *>(t)) == kTypeCorpse && !is_own_corpse(static_cast<char *>(t))) {
      void *actor = ent_actor(static_cast<char *>(t));
      if (actor) {
        hide_actor(actor);
        const uint32_t id = ent_spawnid(static_cast<char *>(t));
        g_hidden[id] = t;
        g_last_hidden_ent = t;
        g_last_hidden_id = id;
      }
    }
  }
  g_loot_orig(lw, edx);
}

// Per-frame (EndScene). When 'always' is on, scans the spawn list and sets the hidden flag on every
// NPC-corpse actor so corpses that spawn later are hidden too. The flag is re-asserted each frame (an
// idempotent byte write; cheap, and robust if the client ever clears it); user-revealed corpses
// (showlast) are skipped so they stay visible. New corpses are recorded so showlast knows the last one.
void on_render(IDirect3DDevice9 * /*dev*/) {
  if (!g_always || !g_writes_armed) return;
  if (crash_handler::shutting_down() || !Rcp::Game::is_in_game()) return;
  void *mgr = *kSpawnManager;
  if (!mgr) return;

  for (void *e = *reinterpret_cast<void **>(static_cast<char *>(mgr) + kListFirst); e;
       e = *reinterpret_cast<void **>(static_cast<char *>(e) + kEntNext)) {
    char *ent = static_cast<char *>(e);
    if (!is_npc_corpse(ent)) continue;    // players, live NPCs, and player corpses: never touched.
    if (!ent_actor(ent)) continue;        // no graphics actor yet.
    const uint32_t id = ent_spawnid(ent);

    auto rev = g_revealed.find(id);
    if (rev != g_revealed.end()) {
      if (rev->second == e) continue;     // user revealed this one via showlast: leave it visible.
      g_revealed.erase(rev);              // spawn-id reused by a different corpse: exemption is stale.
    }

    hide_model(ent);                      // set the hidden flag (idempotent; re-asserted every frame).
    auto hit = g_hidden.find(id);
    if (hit == g_hidden.end() || hit->second != e) {
      g_hidden[id] = e;                   // newly appeared corpse -> becomes the "last hidden".
      g_last_hidden_ent = e;
      g_last_hidden_id = id;
    }
  }
}

void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus /hidecorpses (all client-side + reversible): always=%s looted=%s.",
                        g_always ? "ON" : "off", g_looted ? "ON" : "off");
  Rcp::Game::print_chat("  npc / all / allbutgroup = hide existing NPC / all-but-own / all-but-group corpses now");
  Rcp::Game::print_chat("  always = auto-hide NPC corpses continuously | looted = auto-hide each corpse after you loot it");
  Rcp::Game::print_chat("  showlast = reveal the last hidden corpse | off (or none) = show all corpses again");
}

void do_showlast() {
  if (g_last_hidden_ent && g_last_hidden_id) {
    void *live = get_spawn_by_id(static_cast<int>(g_last_hidden_id));
    if (live && live == g_last_hidden_ent && ent_type(static_cast<char *>(live)) == kTypeCorpse) {
      g_revealed[g_last_hidden_id] = live;
      g_hidden.erase(g_last_hidden_id);
      show_model(static_cast<char *>(live));
      Rcp::Game::print_chat("rof2ClientPlus: revealing last hidden corpse.");
      g_last_hidden_ent = nullptr;
      g_last_hidden_id = 0;
      return;
    }
  }
  Rcp::Game::print_chat("rof2ClientPlus: no hidden corpse to show.");
}

// One-shot: hide every corpse in the given scope that exists right now (client-side, via the same
// reversible flag and tracking as 'always'), WITHOUT turning on continuous mode. Unlike the native
// (server-side, non-reversible) commands, corpses hidden this way are tracked, so showlast reveals the
// last and 'off' brings them all back. all=false -> NPC corpses ('npc'); all=true -> every corpse except
// your own ('all').
void hide_existing(Scope scope) {
  if (!Rcp::Game::is_in_game()) return;
  void *mgr = *kSpawnManager;
  if (!mgr) {
    Rcp::Game::print_chat("hidecorpses: not in a zone.");
    return;
  }
  int n = 0;
  for (void *e = *reinterpret_cast<void **>(static_cast<char *>(mgr) + kListFirst); e;
       e = *reinterpret_cast<void **>(static_cast<char *>(e) + kEntNext)) {
    char *ent = static_cast<char *>(e);
    if (!should_hide(ent, scope)) continue;
    void *actor = ent_actor(ent);
    if (!actor) continue;
    const uint32_t id = ent_spawnid(ent);
    auto rev = g_revealed.find(id);
    if (rev != g_revealed.end() && rev->second == e) continue;  // leave user-revealed corpses visible.
    hide_actor(actor);
    auto hit = g_hidden.find(id);
    if (hit == g_hidden.end() || hit->second != e) {
      g_hidden[id] = e;
      g_last_hidden_ent = e;
      g_last_hidden_id = id;
    }
    ++n;
  }
  const char *label = scope == ScopeNPC ? "existing NPC" : scope == ScopeAllButGroup ? "existing non-group" : "existing";
  Rcp::Game::print_chat("rof2ClientPlus: hid %d %s corpse(s). '/hidecorpses showlast' reveals the last; "
                        "'/hidecorpses off' shows them all.",
                        n, label);
}

}  // namespace

namespace hide_corpse_settings {
bool get_always() { return g_always; }
void set_always(bool on) {
  if (on != g_always) {
    g_always = on;
    save_always();
  }
  // Turning off (or re-affirming off, e.g. after a one-shot 'npc' hide) reveals everything we hid.
  if (!on) reveal_all_hidden();
}
bool get_looted() { return g_looted; }
void set_looted(bool on) {
  g_looted = on;
  save_looted();
}
}  // namespace hide_corpse_settings

HideCorpse::HideCorpse(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  directx::add_render_callback(on_render);
  // Detour CLootWnd::Deactivate for 'looted' mode (acts only when g_looted is on).
  rcp->hooks->Add("hidecorpse_loot", static_cast<int>(kLootDeactivate), loot_deactivate_hk, hook_type_detour);
  g_loot_orig = rcp->hooks->hook_map["hidecorpse_loot"]->original(loot_deactivate_hk);
  logger::logf("[hidecorpse] loaded: always=%d looted=%d (loot detour @0x%x)", (int)g_always, (int)g_looted,
               (unsigned)kLootDeactivate);

  // We REPLACE the stock /hidecorpses command with a fully client-side, reversible implementation: every
  // hide keyword is handled here (never forwarded to the native server-side handler) so hide/showlast/off
  // are all consistent. Returning true always suppresses the native handler.
  //
  // The client accepts any unambiguous command ABBREVIATION (`/hidecor`, `/hideco`, ...), but our command
  // hook only exact-matches the registered name - so without these aliases a user typing `/hidecor always`
  // bypasses us entirely and native (which doesn't know "always") handles it. Register every unambiguous
  // prefix of "/hidecorpses" as an alias (only /hideme and /hidecorpses start with /hide, and they diverge
  // at "/hidec"/"/hidem", so /hidec.. is unambiguous). Covers the singular "/hidecorpse" too.
  rcp->commands_hook->Add(
      "/hidecorpses", {"/hidecorpse", "/hidecorps", "/hidecorp", "/hidecor", "/hideco", "/hidec"},
      "Reversible client-side corpse hiding: npc/all (hide existing NPC / all-but-own corpses now), always "
      "(auto-hide NPC corpses continuously incl. future ones), showlast (reveal the last hidden corpse to "
      "loot it), off/none (show all). Your own corpse is never hidden.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (Rcp::String::compare_insensitive(a, "always")) {
            hide_corpse_settings::set_always(!hide_corpse_settings::get_always());
            print_status();
            return true;  // ours: do not fall through to the native handler.
          }
          if (Rcp::String::compare_insensitive(a, "on")) {
            hide_corpse_settings::set_always(true);
            print_status();
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "off")) {
            hide_corpse_settings::set_always(false);  // reversibly restores every corpse we hid.
            print_status();
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "showlast")) {
            do_showlast();
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "npc")) {  // one-shot: hide existing NPC corpses.
            hide_existing(ScopeNPC);
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "all")) {  // one-shot: hide all existing corpses except your own.
            hide_existing(ScopeAll);
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "allbutgroup")) {  // one-shot: hide all except own + group members'.
            hide_existing(ScopeAllButGroup);
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "looted")) {  // toggle: auto-hide each corpse after you loot it.
            hide_corpse_settings::set_looted(!hide_corpse_settings::get_looted());
            print_status();
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "debug")) {
            do_debug();
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "call")) {  // probe: /hidecorpses call <slot> <arg>
            if (args.size() >= 4)
              do_call((int)strtol(args[2].c_str(), nullptr, 0), (int)strtol(args[3].c_str(), nullptr, 0));
            else
              Rcp::Game::print_chat("usage: /hidecorpses call <vtable_slot_decimal> <arg>");
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "setf")) {  // poke float: /hidecorpses setf <off_hex> <float>
            if (args.size() >= 4)
              do_setf((int)strtol(args[2].c_str(), nullptr, 16), atof(args[3].c_str()));
            else
              Rcp::Game::print_chat("usage: /hidecorpses setf <actor_offset_hex> <float>");
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "testhide")) {  // remove ONE targeted corpse from the scene.
            do_test(true);
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "testshow")) {  // re-add ONE targeted corpse to the scene.
            do_test(false);
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "none")) {  // reveal every corpse we hid + stop continuous mode.
            hide_corpse_settings::set_always(false);
            print_status();
            return true;
          }
          // Unknown argument: print our usage. We handle every corpse-hide keyword ourselves and never fall
          // through to the native (server-side, non-reversible) handler, so the whole command is consistent.
          print_status();
          return true;
        }
        print_status();  // no argument
        return true;
      });
  logger::log("[hidecorpse] /hidecorpses extra options registered");
}

HideCorpse::~HideCorpse() {}
