// rof2ClientPlus - NPC/creature body-model classic-vs-new swap (Phase 2). See npc_model_swap.h.
#include "npc_model_swap.h"

#include <windows.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"  // Rcp::Game::print_chat
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// Body-model build pipeline (RE-confirmed in-game): per spawn / appearance-change, BuildActor
// (F_48ff@0x48f4b0) reads the spawn's actordef name at spawn+0xEBA, resolves it via
// FindActorDefByName@0x4066f0 in the registry@0xDD260C, and on a MISS builds + REGISTERS it via
// CreateActorDefinition@0x408030 (the ONLY populator of that registry). A custom classic alias
// (RCPSKE_ACTORDEF), present in a loaded archive but never spawn-requested, is thus never registered,
// so a Find-arg redirect to it just misses (observed: SKT->RCPSKE fired 204x, no change). THE FIX:
// detour BuildActor and rewrite spawn+0xEBA to the substitute at entry -- the build then misses on the
// alias and CreateActorDefinition builds it from the loaded rcpglobal_chr fragments + registers it, so
// it renders. Rewriting +0xEBA at entry is safe: the internal name-rebuild (SetActorDefName@0x59e4d0)
// is COLD on the normal path (confirmed zero in-game hits), so it never clobbers our value.
constexpr uintptr_t kBuildActor = 0x48f4b0;          // __thiscall(mgr; spawn,p2,p3,p4,p5,p6) 6 stack args, ret 0x18
constexpr uintptr_t kFindActorDefByName = 0x4066f0;  // Node* __stdcall(char* name) ret 4; registry @0xDD260C
constexpr int kEntName = 0xA4;       // char[] spawn name
constexpr int kEntRace = 0xEB4;      // int race
constexpr int kEntActorDef = 0xEBA;  // char[64] actordef name -- the field the build consumes
constexpr int kActorDefCap = 64;
constexpr int kLogCap = 4000;
// Live-refresh: re-invoking the (hooked) BuildActor on a spawn is the game's own live model-rebuild
// (F_48ff releases the old actor@+0x101C then rebuilds from +0xEBA -> hits our detour). Full-body arg
// set (spawn, arg2=0, 1, 2, 1, 0) is what the illusion / auto-build paths use. Iterate live spawns via
// the client's spawn manager. Main-thread only.
constexpr int kEntSpawnId = 0x148;                                 // eqlib spawn id
void **const kSpawnManager = reinterpret_cast<void **>(0xE641D0);  // PlayerManagerClient (GetSpawnByID this)
constexpr uintptr_t kGetSpawnByID = 0x5996E0;
void **const kRenderMgr = reinterpret_cast<void **>(0xDD2660);    // BuildActor's `this` (render mgr)
void **const kLocalPlayer = reinterpret_cast<void **>(0xDD2630);  // pinstLocalPlayer -- never force-rebuild
constexpr int kEntActor = 0x101C;  // CActorInterface* -- the live render actor (shared w/ hide_corpse).
constexpr size_t kMaxSpawns = 4096;

typedef int(__fastcall *BuildActorFn)(void *mgr, int edx, void *spawn, void *p2, int p3, int p4, int p5,
                                      int p6);
BuildActorFn g_build_orig = nullptr;
typedef void *(__stdcall *FindActorDefFn)(char *name);
FindActorDefFn g_find_orig = nullptr;

constexpr char kIniSection[] = "NpcModels";

// Curated catalog of creatures/races whose CURRENT (Luclin) model differs from the classic one -- gated by
// tools/validate_models.py (live DB spawns AND modern-mesh != classic-mesh, three-way vs Trilogy + TAKP).
// Each entry maps its RoF2 actordef code(s) to a same-length classic alias built by isolate_archive.py into
// rcp*.s3d (loaded via GlobalLoad.txt). Player races map each GENDER code to its own alias (HUM->HU9, HUF->HU8).
struct CodeMap {
  const char *from;  // RoF2 code, e.g. "SKE" / "HUM"
  const char *to;    // classic alias in rcpglobal_chr, e.g. "SK9" / "HU9"
};
struct NpcRevamp {
  const char *name;   // display name
  CodeMap maps[4];    // {code, alias} pairs; unused trailing entries zero-fill to {nullptr,nullptr}
  bool pc;            // PC-race model: after a live rebuild the OLD actor keeps an extra ref (scene/equip)
                      // and ghosts unless we destroy it -- refresh_world() drops that ref (see kEntActor).
};
const NpcRevamp kNpcRevamps[] = {
    // --- creatures (global6 Luclin revamps; DRK/ALL/WOF excluded -- proven mesh-identical to classic) ---
    {"Skeleton", {{"SKE", "SK9"}, {"SKT", "SK9"}}},  // SKE = Luclin global skeleton; SKT = zone skeletons
    {"Wolf", {{"WOL", "WO9"}}},
    {"Tiger", {{"TIG", "TI9"}}},
    {"Spirit Wolf", {{"WOE", "WO8"}}},  // race 120 spirit/elemental wolf (shaman/beastlord pets, druid epic)
    // NOTE: player races are NOT here. The actordef alias-redirect can't render PC models correctly -- the
    // client's worn-armor + face-selection systems only recognize the NATIVE race codes (HUM/HUF/...), so an
    // isolated alias (HU9) draws as a bare body with a default face. Confirmed in-game 2026-07-14. PC-race
    // classic<->Luclin must go through the client's own UseLuclin mechanism, not this creature-redirect.
    // (The pc flag + release_actor destroy path stay wired for if that native approach is built later.)
};

std::mutex g_mu;
std::map<std::string, std::string> g_redirect;  // active: "<code>_ACTORDEF" -> "<classic>_ACTORDEF"
std::set<std::string> g_persistent_subs;        // redirect TARGETS that must STAY in spawn+0xEBA after the
                                                // build (PC aliases): runtime anim/appearance lookups key
                                                // off the spawn's actordef name, so restoring the native
                                                // code T-poses a new-style alias body. Guarded by g_mu.
int g_pc_log_count = 0;  // cap for PC-race diagnostics ([pcargs]/[pcmodel] lines)
std::map<std::string, bool> g_on;               // catalog display name -> reverted to classic (persisted)
std::map<std::string, std::string> g_manual;    // /rcpbody discovery redirects (session only)
std::set<int> g_spawn_ids;                      // spawn ids BuildActor has seen -> live-refresh targets
bool g_log = false;
int g_log_count = 0;

std::string upper(std::string s) {
  for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
void read_cstr(const void *p, char *out, int cap) {
  out[0] = '\0';
  const char *s = reinterpret_cast<const char *>(p);
  if (!s) return;
  int i = 0;
  for (; i < cap - 1 && s[i]; ++i) out[i] = (s[i] >= 32 && s[i] < 127) ? s[i] : '?';
  out[i] = '\0';
}
std::string norm_actordef(const std::string &s) {
  std::string u = upper(s);
  if (u.find("_ACTORDEF") == std::string::npos) u += "_ACTORDEF";
  return u;
}

const NpcRevamp *find_revamp(const std::string &name) {
  for (const auto &r : kNpcRevamps)
    if (name == r.name) return &r;
  return nullptr;
}

void add_pc_redirects();  // PC-race alias redirects (defined with the PC section below)

// Rebuild the active redirect map from the catalog on-state + manual /rcpbody entries + PC-race aliases.
// Caller holds g_mu.
void rebuild_redirect() {
  g_redirect.clear();
  for (const auto &r : kNpcRevamps) {
    auto it = g_on.find(r.name);
    if (it == g_on.end() || !it->second) continue;
    for (int i = 0; i < 4 && r.maps[i].from; ++i)
      g_redirect[std::string(r.maps[i].from) + "_ACTORDEF"] = std::string(r.maps[i].to) + "_ACTORDEF";
  }
  for (auto &kv : g_manual) g_redirect[kv.first] = kv.second;
  add_pc_redirects();
}

void persist(const std::string &name, bool on) {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (on)
    ini.setValue<int>(kIniSection, name, 1);
  else
    ini.deleteKey(kIniSection, name);
}

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  std::lock_guard<std::mutex> lk(g_mu);
  g_on.clear();
  for (auto &kv : ini.getSection(kIniSection))  // key = creature name, value = 1
    if (std::atoi(kv.second.c_str()) > 0 && find_revamp(kv.first)) g_on[kv.first] = true;
  rebuild_redirect();
}

void *get_spawn_by_id(int id) {
  void *mgr = *kSpawnManager;
  if (!mgr || id <= 0) return nullptr;
  return reinterpret_cast<void *(__thiscall *)(void *, int)>(kGetSpawnByID)(mgr, id);
}

// Destroy an ORPHANED render actor left over after a live PC-race rebuild. F_48ff builds a NEW actor
// without tearing the old one down (its vtable[0x1D0] "release" is a no-op on PC actors -- confirmed by
// logging: the actor header is unchanged after it), so the old actor stays scene-linked = a visible ghost
// that leaks until zone. Fix, using hide_corpse's in-game-proven graphics knowledge:
//   1) set the actor's per-instance visibility byte (+0xd1 = 1) so it stops DRAWING immediately -- the
//      exact reversible flag hide_corpse confirmed hides a model. Done first (while the object is alive).
//   2) unlink it from the EQGraphicsDX9 scene manager (mgr->vtable[13](actor)) -- the very call the actor's
//      OWN destructor makes; for an orphan whose only remaining hold is the scene this also frees it.
// Caller must ensure `old` is truly orphaned (new actor differs) + pc-only. All guarded by the caller.
constexpr int kActorHiddenFlag = 0xd1;      // hide_corpse: 1 = model hidden (per-actor, reversible).
constexpr uintptr_t kSceneMgrRva = 0x179110;  // EQGraphicsDX9.dll global holding the scene manager ptr.
constexpr int kSceneRemoveSlot = 13;          // mgr vtable slot (+0x34): unlink an actor from all buckets.
void release_actor(void *actor) {
  if (!actor) return;
  *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(actor) + kActorHiddenFlag) = 1;  // (1) stop drawing
  HMODULE dll = GetModuleHandleA("EQGraphicsDX9.dll");
  if (!dll) return;
  void *mgr = *reinterpret_cast<void **>(reinterpret_cast<char *>(dll) + kSceneMgrRva);
  if (!mgr) return;
  void **mvt = *reinterpret_cast<void ***>(mgr);
  reinterpret_cast<void(__thiscall *)(void *, void *)>(mvt[kSceneRemoveSlot])(mgr, actor);  // (2) unlink+free
}

// Re-render visible spawns NOW (no zone): re-invoke the hooked BuildActor per live spawn so its body is
// torn down + rebuilt with the current redirect applied (the game's own illusion/change-form mechanism --
// F_48ff releases the old actor then rebuilds from spawn+0xEBA, which our detour rewrites).
// Called on a redirect change. MAIN-THREAD ONLY (touches the render mgr + actors). Three guards:
//   (1) NEVER force-rebuild the local player -- a raw out-of-band F_48ff on it spawns a duplicate actor
//       and desyncs the attached camera (the game's own change-form path reattaches; ours can't).
//   (2) Only rebuild spawns whose body is one we manage (a catalog/manual code, ON *or* OFF) -- this
//       reverts toggled-off creatures back to modern and leaves every unrelated NPC untouched.
//   (3) For PC-race spawns, DESTROY the leftover actor after the rebuild (release_actor) -- the old PC
//       actor keeps an extra ref and would otherwise ghost + leak until zone (creatures self-clean).
void refresh_world() {
  void *mgr = nullptr;
  void *local = nullptr;
  rcp_guard::run("npcbody.mgr", [&] {
    mgr = *kRenderMgr;
    local = *kLocalPlayer;
  });
  if (!mgr) return;
  std::vector<int> ids;
  std::set<std::string> managed;   // "<CODE>_ACTORDEF" for every catalog + manual code (regardless of state)
  std::set<std::string> pc_codes;  // subset that are PC-race models -> need the leftover-actor destroy
  {
    std::lock_guard<std::mutex> lk(g_mu);
    ids.assign(g_spawn_ids.begin(), g_spawn_ids.end());
    for (const auto &r : kNpcRevamps)
      for (int i = 0; i < 4 && r.maps[i].from; ++i) {
        std::string code = std::string(r.maps[i].from) + "_ACTORDEF";
        managed.insert(code);
        if (r.pc) pc_codes.insert(code);
      }
    for (auto &kv : g_manual) managed.insert(kv.first);
  }
  std::vector<int> dead;
  for (int id : ids) {
    void *spawn = nullptr;
    rcp_guard::run("npcbody.byid", [&] { spawn = get_spawn_by_id(id); });
    if (!spawn) {
      dead.push_back(id);  // despawned -> prune
      continue;
    }
    if (spawn == local) continue;      // guard (1): never rebuild the local player
    bool mine = false, is_pc = false;  // guard (2): only rebuild bodies we manage
    rcp_guard::run("npcbody.match", [&] {
      char nm[kActorDefCap] = {0};
      read_cstr(reinterpret_cast<char *>(spawn) + kEntActorDef, nm, sizeof(nm));
      std::string u = upper(nm);
      mine = nm[0] && managed.count(u) != 0;
      is_pc = mine && pc_codes.count(u) != 0;  // guard (3): PC spawns need leftover-actor destroy
    });
    if (!mine) continue;
    // Snapshot the actor BEFORE the rebuild so we can destroy the PC leftover afterward.
    void *old_actor = nullptr;
    if (is_pc)
      rcp_guard::run("npcbody.oldactor",
                     [&] { old_actor = *reinterpret_cast<void **>(reinterpret_cast<char *>(spawn) + kEntActor); });
    // Full-body rebuild args (spawn, arg2=0, 1, 2, 1, 0); calling the HOOKED address re-applies the
    // redirect via our detour (which save/rewrites/restores spawn+0xEBA, so toggle-off reverts too).
    rcp_guard::run("npcbody.rebuild",
                   [&] { reinterpret_cast<BuildActorFn>(kBuildActor)(mgr, 0, spawn, nullptr, 1, 2, 1, 0); });
    // guard (3): if the rebuild produced a NEW actor, the old PC actor is orphaned but still ref'd/scene-
    // linked (ghost + leak) -- drop its leftover ref so it's destroyed. pc-only: creature actors already
    // hit ref 0 in F_48ff, so re-releasing them would be a use-after-free.
    if (is_pc && old_actor) {
      void *new_actor = nullptr, *old_vt = nullptr, *new_vt = nullptr;
      rcp_guard::run("npcbody.newactor", [&] {
        new_actor = *reinterpret_cast<void **>(reinterpret_cast<char *>(spawn) + kEntActor);
        if (new_actor && new_actor != old_actor) {
          old_vt = *reinterpret_cast<void **>(old_actor);
          new_vt = *reinterpret_cast<void **>(new_actor);
        }
      });
      // DIAGNOSTIC (/rcpbody log on): dump the leftover so we can see whether the release fires and find
      // the real teardown slot. Logs the actor header (a ref-count likely lives here) + vtable slots.
      bool do_log = false;
      {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_log && g_log_count < kLogCap) { do_log = true; ++g_log_count; }
      }
      if (do_log)
        rcp_guard::run("npcbody.pcdump", [&] {
          uint32_t *o = reinterpret_cast<uint32_t *>(old_actor);
          void **ovt = old_vt ? reinterpret_cast<void **>(old_vt) : nullptr;
          logger::logf("[pcdestroy] old=%p new=%p match=%d hdr: %08x %08x %08x %08x %08x %08x %08x %08x",
                       old_actor, new_actor, (int)(old_vt && old_vt == new_vt), o[0], o[1], o[2], o[3], o[4],
                       o[5], o[6], o[7]);
          if (ovt)
            logger::logf("[pcdestroy] vt[0..4]=%p %p %p %p %p  vt[13]=%p vt[116/0x1D0]=%p", ovt[0], ovt[1],
                         ovt[2], ovt[3], ovt[4], ovt[13], ovt[116]);
        });
      // SAFETY: only release when the old actor's vtable still equals the freshly-built actor's (both live
      // same-class actors). A freed/reused old actor won't match -> skip, avoiding use-after-free.
      if (old_vt && old_vt == new_vt) {
        rcp_guard::run("npcbody.destroy_leftover", [&] { release_actor(old_actor); });
        if (do_log)
          rcp_guard::run("npcbody.pcdump2", [&] {
            uint32_t *o = reinterpret_cast<uint32_t *>(old_actor);
            logger::logf("[pcdestroy] after release hdr: %08x %08x %08x %08x", o[0], o[1], o[2], o[3]);
          });
      }
    }
  }
  if (!dead.empty()) {
    std::lock_guard<std::mutex> lk(g_mu);
    for (int id : dead) g_spawn_ids.erase(id);
  }
}

// Detour the per-spawn actor build: if the spawn's actordef name has a redirect, overwrite spawn+0xEBA
// with the substitute BEFORE the original runs. The build then Find-misses the substitute and
// CreateActorDefinition builds + registers it from the loaded alias archive, so it renders.
int __fastcall BuildActor_hk(void *mgr, int edx, void *spawn, void *p2, int p3, int p4, int p5, int p6) {
  if (!crash_handler::shutting_down() && spawn) {
    char name[kActorDefCap] = {0};
    char ename[32] = {0};
    int race = -1;
    int sid = 0;
    bool is_local = false;
    rcp_guard::run("npcbody.read", [&] {
      read_cstr(reinterpret_cast<char *>(spawn) + kEntActorDef, name, sizeof(name));
      read_cstr(reinterpret_cast<char *>(spawn) + kEntName, ename, sizeof(ename));
      race = *reinterpret_cast<int *>(reinterpret_cast<char *>(spawn) + kEntRace);
      sid = *reinterpret_cast<int *>(reinterpret_cast<char *>(spawn) + kEntSpawnId);
      is_local = spawn == *kLocalPlayer;  // never redirect the local player's own model: it builds
                                          // naturally at login/zone and we can't yet live-rebuild it
                                          // (camera), so a redirect would STICK until the map changes
    });
    if (name[0] && !is_local) {
      std::string sub;
      bool do_log = false;
      {
        std::lock_guard<std::mutex> lk(g_mu);
        if (sid > 0 && (g_spawn_ids.size() < kMaxSpawns || g_spawn_ids.count(sid)))
          g_spawn_ids.insert(sid);  // remember this spawn for live-refresh
        if (g_log && g_log_count < kLogCap) {
          do_log = true;
          ++g_log_count;
        }
        auto it = g_redirect.find(upper(name));
        if (it != g_redirect.end()) sub = it->second;
      }
      if (do_log) {
        if (sub.empty())
          logger::logf("[npcbody] build '%s' race=%d actordef='%s'", ename, race, name);
        else
          logger::logf("[npcbody] build '%s' race=%d actordef='%s' -> '%s'", ename, race, name, sub.c_str());
      }
      // Capture the NATURAL build args for PC-race spawns (login/zone spawn-adds): our manual rebuild must
      // replicate these exactly -- the guessed (p2=+0x2bc, 1,2,1,0) call rebuilds the body but loses the
      // dressed look, so one of these differs. Logged once per session start (cap shared w/ pc logs).
      if (race >= 1 && race <= 12 && g_pc_log_count < 120) {
        ++g_pc_log_count;
        rcp_guard::run("npcbody.state", [&] {
          char *base = reinterpret_cast<char *>(spawn);
          uint8_t bt = *reinterpret_cast<uint8_t *>(base + 0xeb9);   // body texture
          uint8_t ea8 = *reinterpret_cast<uint8_t *>(base + 0xea8);  // ActorBase.TextureType
          uint32_t src[9], wrk[9];  // Equipment@0x1da0 vs ActorEquipment@0xf30 -- .type @+0 (the field the
                                    // wire material actually lands in; +8 was the wrong dword)
          for (int s = 0; s < 9; ++s) {
            src[s] = *reinterpret_cast<uint32_t *>(base + 0x1da0 + s * 0x14);
            wrk[s] = *reinterpret_cast<uint32_t *>(base + 0xf30 + s * 0x14);
          }
          logger::logf("[pcargs] BUILD-TIME id=%d race=%d bt=%d ea8=0x%02x ret=%p src=%u,%u,%u,%u,%u,%u,%u,%u,%u wrk=%u,%u,%u,%u,%u,%u,%u,%u,%u",
                       sid, race, (int)bt, ea8, __builtin_return_address(0), src[0], src[1], src[2], src[3],
                       src[4], src[5], src[6], src[7], src[8], wrk[0], wrk[1], wrk[2], wrk[3], wrk[4],
                       wrk[5], wrk[6], wrk[7], wrk[8]);
        });
      }
      if (!sub.empty() && sub.size() < static_cast<size_t>(kActorDefCap)) {
        // Hand the build the substitute. Creature subs are RESTORED afterward (so a later toggle-off
        // rebuilds from the true code); PC-alias subs are PERSISTENT (see g_persistent_subs) -- the
        // classic-direction rebuild writes the native code back explicitly in pc_reapply.
        bool persist;
        {
          std::lock_guard<std::mutex> lk(g_mu);
          persist = g_persistent_subs.count(sub) != 0;
        }
        char saved[kActorDefCap] = {0};
        bool did = false;
        rcp_guard::run("npcbody.write", [&] {
          char *def = reinterpret_cast<char *>(spawn) + kEntActorDef;
          std::snprintf(saved, sizeof(saved), "%s", def);
          std::snprintf(def, kActorDefCap, "%s", sub.c_str());
          did = true;
        });
        int r = g_build_orig(mgr, edx, spawn, p2, p3, p4, p5, p6);
        if (did && !persist)
          rcp_guard::run("npcbody.restore", [&] {
            std::snprintf(reinterpret_cast<char *>(spawn) + kEntActorDef, kActorDefCap, "%s", saved);
          });
        return r;
      }
    }
  }
  return g_build_orig(mgr, edx, spawn, p2, p3, p4, p5, p6);
}

// THE RACE->CODE STRING BUILDER 0x50f070 (__cdecl(char *buf, int race, int variant) -> char*): the single
// seam every race-keyed name construction flows through -- armor MATERIAL names ("%s%02d01_MDF" @0x409b4e
// with the base built here from ActorBase race/+0x15), hair/head piece names, SetBodyTexture's variant
// handle, the config's global<code>_chr names, def-create tails. Detour: run the original, then if the
// produced native code (HUM/HUF/...) belongs to a PC race toggled MODERN, overwrite it IN PLACE with the
// same-length alias (HU7/HU6/...) -- so armor materials resolve HU7CH..01_MDF (present in the alias
// archives), hair resolves HU7HE.., etc. Classic-toggled races pass through untouched.
// MODEL-TABLE ENTRY code swap. 0x50f070(buf, race, gender) builds a lookup KEY (returns an object, NOT a
// code string -- learn log proved the returned bytes are binary); 0x50a2d0(this=key) resolves it to the
// MODEL TABLE ENTRY registered at startup (0x50a440 run) -- the entry holds a POINTER to the code string
// ("HUM" in .rdata). Every race-keyed name construction (armor materials, hair pieces, SetBodyTexture
// handles, def-create) reads the code THROUGH this entry, so swapping that one pointer per toggle makes the
// whole client alias-keyed for the race. The field offset is DISCOVERED at runtime: scan the entry's first
// dwords for a pointer whose target string equals the native code (or a previously-swapped alias).
typedef char *(__cdecl *CodeBuildFn2)(char *buf, int race, int gender);
typedef void *(__thiscall *KeyLookupFn)(void *key);
constexpr uintptr_t kKeyLookup = 0x50a2d0;

// Set the model-table code for (race, gender) to `to` ("HU7" literal from kPcRaces / native code). Returns
// the offset used (for the log) or -1.
int g_table_dumped = 0;
std::map<void *, uint32_t> g_table_orig_flags;  // record base -> original +0x2C flags (restore on classic)
// Registry unlink (re-added, proven-safe walk): free an actordef NAME so the next build re-creates its def
// (needed when the record's new-style flag changes -- the def bakes the anim-bank choice at creation).
void unlink_actordef_name(const std::string &full) {
  rcp_guard::run("pc.unlink", [&] {
    char **head = reinterpret_cast<char **>(0xDD260C);
    char *prev = nullptr, *cur = *head;
    while (cur) {
      char *next = *reinterpret_cast<char **>(cur + 0x4);
      if (upper(cur + 0x8) == full) {
        if (prev)
          *reinterpret_cast<char **>(prev + 0x4) = next;
        else
          *head = next;
        return;
      }
      prev = cur;
      cur = next;
    }
  });
}
int swap_table_code(int race, int gender, const char *from_a, const char *from_b, const char *to) {
  int found = -1;
  rcp_guard::run("pc.tableswap", [&] {
    char keybuf[0x40] = {0};
    char *key = reinterpret_cast<CodeBuildFn2>(0x50f070)(keybuf, race, gender);
    void *entry = reinterpret_cast<KeyLookupFn>(kKeyLookup)(key);
    if (g_table_dumped < 2) {  // one-shot layout dump so the field stops being a guess
      ++g_table_dumped;
      uint32_t *e = reinterpret_cast<uint32_t *>(entry);
      if (entry)
        logger::logf("[pcmodel] entry race=%d g=%d @%p: %08x %08x %08x %08x %08x %08x %08x %08x | %08x %08x "
                     "%08x %08x %08x %08x %08x %08x",
                     race, gender, entry, e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9], e[10],
                     e[11], e[12], e[13], e[14], e[15]);
      else
        logger::logf("[pcmodel] entry race=%d g=%d LOOKUP RETURNED NULL (key=%p)", race, gender, key);
    }
    if (!entry) return;
    char *base = reinterpret_cast<char *>(entry);
    // The lookup returns record+0xC; the record's INLINE code lives at record+0x8 = entry-0x4 (dump-proven:
    // "HUF" at the next record's +0x8). Scan a window starting BEFORE the returned pointer.
    for (int off = -0x10; off <= 0x80; ++off) {  // byte-granular: catch INLINE codes too
      // inline match: the 3-char code embedded directly in the entry
      auto imatch = [&](const char *code) {
        return code && base[off] == code[0] && base[off + 1] == code[1] && base[off + 2] == code[2] &&
               base[off + 3] == '\0';
      };
      if (imatch(from_a) || imatch(from_b)) {
        std::memcpy(base + off, to, 3);  // same-length inline swap
        // The record starts 0x8 before its inline code; +0x2C holds the model flags. Bit0 = "new-style
        // (Luclin) model" -- what IsActorUsingNewStyleModel/vt-flag getters read. Set it when switching to
        // the alias (Luclin body: correct 122-anim bank + head attach), restore the original when
        // switching back to the native code.
        char *record = base + off - 0x8;
        uint32_t *flags = reinterpret_cast<uint32_t *>(record + 0x2c);
        bool to_alias = to == from_b;  // from_b = the alias code
        if (!g_table_orig_flags.count(record)) g_table_orig_flags[record] = *flags;
        *flags = to_alias ? (*flags | 1u) : g_table_orig_flags[record];
        found = off;
        return;
      }
      if (off >= 0 && off % 4 == 0) {  // pointer match on aligned dwords
        char *s = *reinterpret_cast<char **>(base + off);
        if (s && reinterpret_cast<uintptr_t>(s) >= 0x10000) {
          char b4[4] = {0, 0, 0, 1};
          rcp_guard::run("pc.tableread", [&] {
            b4[0] = s[0];
            b4[1] = s[1];
            b4[2] = s[2];
            b4[3] = s[3];
          });
          auto match = [&](const char *code) {
            return code && b4[0] == code[0] && b4[1] == code[1] && b4[2] == code[2] && b4[3] == '\0';
          };
          if (match(from_a) || match(from_b)) {
            *reinterpret_cast<char **>(base + off) = const_cast<char *>(to);
            found = off;
            return;
          }
        }
      }
    }
  });
  return found;
}

// LEARN-HOOKS #2 (temporary): which def-build path does an alias take? 0x407800 = classic build (0x18-anim
// bank, ret 8), 0x407dc0 = new-style build (0x7A-anim bank, ret 0xC). CreateActorDefinition branches on the
// source object's IsNewStyle -- if HU7 lands in the classic builder, that's the T-pose/no-hair root.
typedef int(__fastcall *DefB2Fn)(void *self, int edx, int a1, int a2);
typedef int(__fastcall *DefB3Fn)(void *self, int edx, int a1, int a2, int a3);
DefB2Fn g_defb_classic = nullptr;
DefB3Fn g_defb_new = nullptr;
int g_defb_log = 0;
std::map<void *, void *> g_def_src;  // def -> the source object the def-build introspected (its a1);
                                     // the vt[0x19C]/vt[0x1A4] attach-slot API lives on THIS object.
int __fastcall DefBuildClassic_hk(void *self, int edx, int a1, int a2) {
  {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_def_src.size() > 4096) g_def_src.clear();
    g_def_src[self] = reinterpret_cast<void *>(a1);
  }
  if (g_defb_log < 60) {
    ++g_defb_log;
    char nm[24] = {0};
    read_cstr(reinterpret_cast<void *>(a2), nm, sizeof(nm));
    logger::logf("[defb] CLASSIC build def=%p a1=%x a2='%s' ret=%p", self, a1, nm,
                 __builtin_return_address(0));
  }
  return g_defb_classic(self, edx, a1, a2);
}
int __fastcall DefBuildNew_hk(void *self, int edx, int a1, int a2, int a3) {
  if (g_defb_log < 60) {
    ++g_defb_log;
    logger::logf("[defb] NEW build def=%p a1=%x a2=%x a3=%x ret=%p", self, a1, a2, a3,
                 __builtin_return_address(0));
  }
  return g_defb_new(self, edx, a1, a2, a3);
}

// LEARN-HOOKS (temporary): passthrough detours on the dress chain, logging the client's OWN natural calls
// (caller ret addr + args) so the re-dress after our rebuild can replicate them exactly. No behavior change.
typedef void(__fastcall *Apply1Fn)(void *self, int edx, int slot);
typedef int(__fastcall *Set6Fn)(void *self, int edx, int a1, int a2, int a3, int a4, int a5, int a6);
typedef void(__fastcall *Slot1Fn)(void *self, int edx, int slot);
Apply1Fn g_apply_orig = nullptr;
Set6Fn g_set6_orig = nullptr;
Slot1Fn g_slotref_orig = nullptr;
int g_learn_count = 0;
void __fastcall ApplyArmor_hk(void *self, int edx, int slot) {
  if (g_learn_count < 300) {
    ++g_learn_count;
    logger::logf("[learn] apply this=%p slot=%d ret=%p", self, slot, __builtin_return_address(0));
  }
  g_apply_orig(self, edx, slot);
}
int __fastcall Set6_hk(void *self, int edx, int a1, int a2, int a3, int a4, int a5, int a6) {
  if (g_learn_count < 300) {
    ++g_learn_count;
    logger::logf("[learn] set6 spawn=%p args=%d,%d,%d,%d,%d,%d ret=%p", self, a1, a2, a3, a4, a5, a6,
                 __builtin_return_address(0));
  }
  return g_set6_orig(self, edx, a1, a2, a3, a4, a5, a6);
}
void __fastcall SlotRefresh_hk(void *self, int edx, int slot) {
  if (g_learn_count < 300) {
    ++g_learn_count;
    logger::logf("[learn] slotref spawn=%p slot=%d ret=%p", self, slot, __builtin_return_address(0));
  }
  g_slotref_orig(self, edx, slot);
}

// Log-only detour on the read-side lookup: shows whether a name resolves in the registry, so we can
// watch a freshly-redirected alias go MISSING (first build) -> resident (subsequent builds).
void *__stdcall FindActorDef_hk(char *name) {
  if (crash_handler::shutting_down() || !name) return g_find_orig(name);
  char nm[kActorDefCap] = {0};
  rcp_guard::run("npcbody.find", [&] { read_cstr(name, nm, sizeof(nm)); });
  bool do_log = false;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_log && g_log_count < kLogCap) {
      do_log = true;
      ++g_log_count;
    }
  }
  void *r = g_find_orig(name);
  if (do_log) logger::logf("[npcbody] Find '%s' : %s", nm, r ? "resident" : "MISSING");
  return r;
}

// ---- PLAYER-RACE classic<->Luclin via the client's OWN mechanism (NOT the actordef alias-redirect above,
// which can't render PC worn-armor/faces). ShouldUseLuclinModel F_48e510@0x48e510 (__thiscall(mgr; race,
// gender), ret 8 -> bool: 1=Luclin globalXXX_chr, 0=classic global_chr) is the single decision the client's
// model config consults per PC race. Detour it to return OUR per-race setting, ignoring the eqclient.ini
// UseLuclin flags -> the NATIVE model loads (armor + face intact). Race ids are the standard EQ ids the
// function receives (it special-cases race 1/4/0x82). Applies when the config next runs (zone); the instant
// re-apply builds on this. ----
constexpr uintptr_t kShouldUseLuclin = 0x48e510;
typedef int(__fastcall *ShouldLuclinFn)(void *mgr, int edx, int race, int gender);
ShouldLuclinFn g_pc_orig = nullptr;

struct PcRace {
  const char *name;
  int race;              // standard EQ race id
  const char *codes[2];  // male / female NATIVE actordef codes
  const char *alias[2];  // male / female SAME-LENGTH substitute codes used when the race is toggled off
                         // its native style (transient redirect in BuildActor_hk). null = no alias yet.
};
// The aliases are the LUCLIN bodies renamed to unique same-length codes (male <2ch>7, female <2ch>6),
// built by isolate_archive.py from each global<code>_chr.s3d into rcp<alias>.s3d (24 archives, GlobalLoad
// "3,0,TFFFC,rcpXX7," lines; per-archive RCP<alias> label prefix so generic labels can't collide). With the
// user's all-classic ini, native = classic and "modern" redirects to the Luclin alias -- ini fully ignored.
// (Erudite/High Elf/Dark Elf/Half Elf archives are small -- they natively borrow the shared new-style anim
// bank -- watch those four for animation quirks.)
const PcRace kPcRaces[] = {
    {"Human", 1, {"HUM", "HUF"}, {"HU7", "HU6"}},
    {"Barbarian", 2, {"BAM", "BAF"}, {"BA7", "BA6"}},
    {"Erudite", 3, {"ERM", "ERF"}, {"ER7", "ER6"}},
    {"Wood Elf", 4, {"ELM", "ELF"}, {"EL7", "EL6"}},
    {"High Elf", 5, {"HIM", "HIF"}, {"HI7", "HI6"}},
    {"Dark Elf", 6, {"DAM", "DAF"}, {"DA7", "DA6"}},
    {"Half Elf", 7, {"HAM", "HAF"}, {"HA7", "HA6"}},
    {"Dwarf", 8, {"DWM", "DWF"}, {"DW7", "DW6"}},
    {"Troll", 9, {"TRM", "TRF"}, {"TR7", "TR6"}},
    {"Ogre", 10, {"OGM", "OGF"}, {"OG7", "OG6"}},
    {"Halfling", 11, {"HOM", "HOF"}, {"HO7", "HO6"}},
    {"Gnome", 12, {"GNM", "GNF"}, {"GN7", "GN6"}},
};
std::map<int, bool> g_pc_classic;  // race id -> force classic (true) / force Luclin (false). Persisted.
constexpr char kPcIniSection[] = "PcModels";

const PcRace *find_pc_by_name(const std::string &name) {
  for (const auto &r : kPcRaces)
    if (upper(name) == upper(r.name)) return &r;
  return nullptr;
}

int g_pc_hook_hits = 0;  // DIAGNOSTIC: count/log hook fires so we can see if the config re-runs on zone.
int __fastcall ShouldUseLuclin_hk(void *mgr, int edx, int race, int gender) {
  if (!crash_handler::shutting_down()) {
    bool classic = false, have = false;
    rcp_guard::run("pcluclin.decide", [&] {
      std::lock_guard<std::mutex> lk(g_mu);
      auto it = g_pc_classic.find(race);
      if (it != g_pc_classic.end()) {
        classic = it->second;
        have = true;
      }
      if (g_pc_hook_hits < 400) {
        ++g_pc_hook_hits;
        logger::logf("[pcmodel] ShouldUseLuclin FIRED race=%d gender=%d have=%d -> %d", race, gender,
                     (int)have, have ? (classic ? 0 : 1) : -1);
      }
    });
    if (have) return classic ? 0 : 1;  // OUR choice, ignore ini: 0=classic global_chr, 1=Luclin globalXXX_chr
  }
  return g_pc_orig(mgr, edx, race, gender);
}

void pc_persist(const std::string &name, bool classic) {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (!classic)
    ini.setValue<int>(kPcIniSection, name, 1);  // store only the non-default (modern/Luclin) overrides
  else
    ini.deleteKey(kPcIniSection, name);
}

void pc_load() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  std::lock_guard<std::mutex> lk(g_mu);
  g_pc_classic.clear();
  if (ini.getValue<int>(kPcIniSection, "Native") > 0) {
    rebuild_redirect();  // DEBUG native mode: empty map -> hook falls through to the eqclient.ini
    return;
  }
  for (const auto &r : kPcRaces) g_pc_classic[r.race] = true;  // default: force classic (ignore the ini)
  for (auto &kv : ini.getSection(kPcIniSection))
    if (const auto *pr = find_pc_by_name(kv.first))
      if (std::atoi(kv.second.c_str()) > 0) g_pc_classic[pr->race] = false;  // persisted modern/Luclin
  rebuild_redirect();  // fold the PC alias redirects into the live map (g_mu held)
}

// PC-race entries for g_redirect (see rebuild_redirect): when a race is toggled OFF its native style,
// redirect each gender's native code to its alias so BuildActor_hk hands the DLL the alias body. The
// swap is transient (spawn+0xEBA restored after the build), so armor/face keying stays native. Races
// without alias archives yet contribute nothing. Caller holds g_mu.
void add_pc_redirects() {
  g_persistent_subs.clear();
  for (const auto &r : kPcRaces) {
    for (int g = 0; g < 2; ++g)  // every PC alias is a persistent sub whenever it is active
      if (r.alias[g]) g_persistent_subs.insert(std::string(r.alias[g]) + "_ACTORDEF");
    auto it = g_pc_classic.find(r.race);
    if (it == g_pc_classic.end() || it->second) continue;  // native/classic -> no redirect
    for (int g = 0; g < 2; ++g)
      if (r.alias[g]) {
        g_redirect[std::string(r.codes[g]) + "_ACTORDEF"] = std::string(r.alias[g]) + "_ACTORDEF";
      }
  }
}

// DEAD ENDS, kept for the record (each was proven in-game 2026-07-14):
//   * actordef registry @0xDD260C (linked list, FindActorDefByName@0x4066f0, inline name @node+8, FIRST
//     match wins): unlink/relink surgery works mechanically, but the registry is populated LAZILY -- the
//     def is created on a BuildActor lookup miss by INTROSPECTING THE LIVE ACTOR INSTANCE (F_48ff miss ->
//     new(0x5c) -> CreateActorDefinition@0x408030, which enumerates tracks off the instance object from
//     0x58ce70(spawn), def cache @spawn+0x1024). So the def follows whatever instance the DLL built; the
//     actual classic-vs-Luclin choice is the graphics DLL's own name->fragment table, which is ALSO
//     first-wins (stock Luclin only works because the config loads globalXXX_chr BEFORE global_chr at
//     startup -- GlobalLoad phase order). Flipping it live would mean DLL-heap surgery.
//   * 0x518530 is NOT an archive loader -- it paints the loading screen (random tip via 0x4a59a0).
//
// WORKING MECHANISM instead: the same-length transient alias redirect our creature swap already uses.
// F_48ff copies spawn+0xEBA into a LOCAL buffer at entry (first GetActorDefName@0x59e4c0 caller, strcpy to
// esp+0x30) and builds the instance from that copy -- so BuildActor_hk's swap->build->restore gives the
// DLL an alias body while every persistent eqgame-side field keeps the NATIVE code (armor/face keying
// intact). Worn armor is a separate pass anyway: ANY rebuild strips it because dressing happens outside
// F_48ff -- ApplyArmorTexture@0x40e450 (__thiscall this=spawn+0xEA4 appearance sub-object, int slot;
// slot=-1 -> all slots; race gate inside is exactly the playable races; robes 10..16 handled). The
// appearance object is inline at spawn+0xEA4 (its +0x10 race == our proven Race@0xeb4). Re-calling it
// after the rebuild re-dresses the new actor.
constexpr uintptr_t kApplyArmor = 0x40e450;        // ApplyArmorTexture(this=spawn+0xEA4; int slot), -1 = all
constexpr uintptr_t kApplyBodyTexture = 0x594ad0;  // __thiscall(spawn): re-apply the NPC fixed body texture
                                                   // (spawn+0xEA8 byte 1..0x1E); no-op for 0xFF/equipment
constexpr int kEntWearObj = 0xea4;                 // inline appearance/equipment sub-object on the spawn
// Worn-equipment data lives TWICE (eqlib exact-build structs): the server truth `EQUIPMENT Equipment` at
// spawn+0x1DA0 (survives rebuilds) and the appearance obj's working copy `ActorEquipment` at +0x8C
// (= spawn+0xF30) which is what ApplyArmorTexture actually reads -- and which the in-game dump showed EMPTY
// after our out-of-band rebuild (the natural spawn-add path is what fills it). Re-copy source->working
// before dressing. EQUIPMENT = 9 x ArmorProperties{type,variation,material,newArmorID,newArmorType}.
constexpr int kEntEquipSrc = 0x1da0;
constexpr int kWearEquip = 0x8c;
constexpr int kEquipSize = 0xb4;
// The NPC "armor" look on most PC-race NPCs is a BODY-TEXTURE VARIANT of the model (npc_types.texture
// 1=leather.., 343/631 human NPCs have 1): the spawn decoder calls SetBodyTexture@0x59e400(spawn; tex) --
// it stores the byte at spawn+0xEB9 (right before the actordef name; NOT eqlib's ActorBase 'Gender' label)
// and resolves the race+texture variant handle into spawn+0x234 (same 0x50f070 name-builder the def
// creation uses; F_48ff's create path reads 0xEB9 @0x49000b). Re-run it before our rebuild so the fresh
// instance picks the variant up again.
constexpr uintptr_t kSetBodyTexture = 0x59e400;
constexpr uintptr_t kRefreshEquipSlot = 0x594e50;  // __thiscall(spawn; int slot): native per-slot equipment
                                                   // refresh (illusion handler's primitive; no packet sends)
constexpr uintptr_t kSet6 = 0x594de0;  // __thiscall(spawn; slot, material, 0, tintARGB, 5, 0) ret 0x18 --
                                       // the per-slot dress setter the natural spawn-add emits (learn log)
constexpr int kEntBodyTex = 0xeb9;
constexpr int kEntBodyTexHandle = 0x234;
// SetRace@0x59e440 = __thiscall(spawn; int race): stores race@0xEB4 + re-resolves the race+texture variant
// handle @+0x234 + pokes anim state. NO rebuild, NO dress (disasm-complete) -- kept for reference.
constexpr uintptr_t kSetRace = 0x59e440;
// EnsureActor@0x5a1a40 = __thiscall(spawn), no args, plain ret: THE natural "build my actor" wrapper -- the
// exact path every login/zone-entry spawn build takes (hook return address 0x5a1baa = its F_48ff call site;
// callers: world-entry loop 0x51c76b + spawn-add 0x59673b). No-ops when spawn+0x101C already has an actor;
// on a missing actor it runs the full natural sequence: appearance-obj prelude (0x40f090 / SetOwner
// 0x40c160 / 0x40c1a0) -> F_48ff (our redirect detour applies) -> post-init (glow/scale). Natural builds
// through it come out DRESSED, so detaching the actor and calling it replicates login exactly.
constexpr uintptr_t kEnsureActor = 0x5a1a40;

// DIAGNOSTIC: dump the appearance obj + material records (spawn+0xEA4..+0xFA4) so a before/after diff shows
// exactly which fields F_48ff resets (the armor re-dress reads these).
void dump_wear(void *spawn, const char *tag, int id) {
  rcp_guard::run("pc.dump", [&] {
    for (int row = 0; row < 4; ++row) {
      uint32_t *p = reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(spawn) + kEntWearObj + row * 0x40);
      logger::logf("[pcdump] id=%d %s +%03x: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x "
                   "%08x %08x %08x %08x",
                   id, tag, row * 0x40, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10],
                   p[11], p[12], p[13], p[14], p[15]);
    }
  });
}
constexpr int kEntAppearance = 0x2bc;  // appearance/equipment source object -- F_48ff's p2 (natural build
                                       // passes it; a null p2 rebuilds a BARE body, stripping worn armor).

void reapply_armor(void *spawn) {
  rcp_guard::run("pc.rewear", [&] {
    reinterpret_cast<void(__thiscall *)(void *, int)>(kApplyArmor)(
        reinterpret_cast<char *>(spawn) + kEntWearObj, -1);
  });
}

void pc_reapply() {
  void *mgr = nullptr, *local = nullptr;
  rcp_guard::run("pc.mgr", [&] {
    mgr = *kRenderMgr;
    local = *kLocalPlayer;
  });
  if (!mgr) return;
  // 1) Sync the alias redirects to the current per-race settings, and point each toggled race's MODEL-TABLE
  //    entry code at the alias (or back at the native code) -- armor materials / hair pieces / texture
  //    handles are all constructed from that entry's code string (see swap_table_code).
  {
    std::lock_guard<std::mutex> lk(g_mu);
    rebuild_redirect();
  }
  for (const auto &r : kPcRaces) {
    bool classic;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      classic = g_pc_classic[r.race];
    }
    for (int g = 0; g < 2; ++g) {
      if (!r.alias[g]) continue;
      const char *to = classic ? r.codes[g] : r.alias[g];
      int off = swap_table_code(r.race, g, r.codes[g], r.alias[g], to);
      unlink_actordef_name(std::string(r.alias[g]) + "_ACTORDEF");  // stale def baked the old anim bank
      if (g_pc_log_count < 200) {
        ++g_pc_log_count;
        logger::logf("[pcmodel] table race=%d g=%d -> '%s' (field @+0x%x)", r.race, g, to, off);
      }
    }
  }
  // 2) Rebuild visible PC-race spawns (BuildActor_hk applies/clears the alias per the map), passing the
  //    spawn's appearance object as p2 (the natural build's parameter). Destroy the leftover actor, then
  //    RE-DRESS the new one (ApplyArmorTexture) -- dressing is a separate pass the rebuild always strips.
  std::map<std::string, const PcRace *> match;      // native AND alias actordef names -> race entry
  std::map<std::string, std::string> alias_native;  // alias full name -> native full name (per gender)
  std::vector<int> ids;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    ids.assign(g_spawn_ids.begin(), g_spawn_ids.end());
    for (const auto &r : kPcRaces)
      for (int g = 0; g < 2; ++g) {
        std::string native = std::string(r.codes[g]) + "_ACTORDEF";
        match[native] = &r;
        if (r.alias[g]) {
          std::string alias = std::string(r.alias[g]) + "_ACTORDEF";
          match[alias] = &r;
          alias_native[alias] = native;
        }
      }
  }
  std::vector<int> dead;
  int reapply_logged = 0;
  for (int id : ids) {
    void *spawn = nullptr;
    rcp_guard::run("pc.byid", [&] { spawn = get_spawn_by_id(id); });
    if (!spawn) {
      dead.push_back(id);
      continue;
    }
    if (spawn == local) continue;
    bool mine = false;
    const PcRace *pr = nullptr;
    std::string cur;
    rcp_guard::run("pc.match", [&] {
      char nm[kActorDefCap] = {0};
      read_cstr(reinterpret_cast<char *>(spawn) + kEntActorDef, nm, sizeof(nm));
      cur = upper(nm);
      auto it = match.find(cur);
      if (it != match.end()) {
        mine = true;
        pr = it->second;
      }
    });
    if (!mine) continue;
    // PERSISTENT alias bookkeeping: if the spawn currently carries an ALIAS name and the race is being
    // toggled CLASSIC, write the native code back BEFORE the rebuild (the alias is persistent in +0xEBA
    // by design -- runtime anim/appearance lookups key off it; see g_persistent_subs). Modern-direction
    // spawns keep/gain the alias via the BuildActor_hk redirect.
    bool wanted_classic;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      wanted_classic = g_pc_classic[pr->race];
    }
    auto an = alias_native.find(cur);
    if (wanted_classic && an != alias_native.end())
      rcp_guard::run("pc.nativename", [&] {
        std::snprintf(reinterpret_cast<char *>(spawn) + kEntActorDef, kActorDefCap, "%s",
                      an->second.c_str());
      });
    // Rebuild = replicate login exactly: detach the actor (EnsureActor only builds on a missing actor),
    // run the NATURAL build wrapper (full prelude + hooked F_48ff -> alias redirect applies + post-init,
    // i.e. everything a dressed login build gets), then destroy the detached orphan (vtable-match guard).
    void *old_actor = nullptr;
    rcp_guard::run("pc.detach", [&] {
      char *base = reinterpret_cast<char *>(spawn);
      old_actor = *reinterpret_cast<void **>(base + kEntActor);
      *reinterpret_cast<void **>(base + kEntActor) = nullptr;
    });
    rcp_guard::run("pc.ensure",
                   [&] { reinterpret_cast<void(__thiscall *)(void *)>(kEnsureActor)(spawn); });
    // Re-dress: the wear state arrives via server WearChange packets after spawn-add and persists in
    // Equipment@0x1DA0 / ActorEquipment@0xF30 (.type field = the wire material). Refresh the working copy
    // from the source, then run both native apply paths (each self-guarded).
    uint32_t pre0 = 0, post0 = 0;
    void *wearactor = nullptr;
    rcp_guard::run("pc.redress", [&] {
      char *base = reinterpret_cast<char *>(spawn);
      std::memcpy(base + kEntWearObj + kWearEquip, base + kEntEquipSrc, kEquipSize);
      pre0 = *reinterpret_cast<uint32_t *>(base + kEntWearObj + kWearEquip + 0x14);  // chest .type in
      wearactor = *reinterpret_cast<void **>(base + kEntWearObj + 0x178);            // the actor it dresses
      // Replicate the natural per-slot dress EXACTLY as captured by the learn-hooks (login spawn-add,
      // caller 0x595ccf): for each armor slot 0..6 with a nonzero material, call the 6-arg setter
      // set6@0x594de0(spawn; slot, material, 0, tintARGB, 5, 0) -- material from ActorEquipment[slot].type
      // (just refreshed from Equipment@0x1DA0 above), tint from ArmorColor[slot]@spawn+0xEFC. (The 0x594e50
      // wrapper loop AV'd when called out of context; the setter itself is the observed workhorse.)
      for (int slot = 0; slot <= 6; ++slot) {
        uint32_t mat = *reinterpret_cast<uint32_t *>(base + kEntWearObj + kWearEquip + slot * 0x14);
        uint32_t tint = *reinterpret_cast<uint32_t *>(base + kEntWearObj + 0x58 + slot * 4);
        if (mat)
          reinterpret_cast<int(__thiscall *)(void *, int, int, int, int, int, int)>(kSet6)(
              spawn, slot, static_cast<int>(mat), 0, static_cast<int>(tint), 5, 0);
      }
      reinterpret_cast<void(__thiscall *)(void *)>(kApplyBodyTexture)(spawn);
      // HEAD/HAIR -- three natural passes our redress was missing:
      //  1. 0x40b7f0(ActorBase): registers the attach-point slots on the actor (sprintf "%sHAIR_POINT_DAG"
      //     etc. from the ActorBase state -> pActor->vt[0x19C](slot, dagname)); natural site 0x5960a7.
      //  2. 0x58de30(spawn; helm=-1): attaches the head/helm mesh (gates on def+0x34 and race).
      //  3. ApplyFace@0x40ecf0(ActorBase; headStyle@spawn+0xEB8): hair/beard/face styling (site 0x4903db).
      reinterpret_cast<void(__thiscall *)(void *)>(0x40b7f0)(base + kEntWearObj);
      // The def's head-capable flag (+0x34) is set by a TUNIC_POINT_DAG probe during def-create that fails
      // for alias rebuilds (wrapper quirk) -- but it is a DATA property and the alias archives carry the
      // attach dags verbatim (offset-stable rename of the native Luclin data). Assert it ourselves so the
      // head attach below can run.
      void *cdef = *reinterpret_cast<void **>(base + 0x1024);
      if (cdef) *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(cdef) + 0x34) = 1;
      reinterpret_cast<void(__thiscall *)(void *, int)>(0x58de30)(spawn, -1);
      uint8_t head = *reinterpret_cast<uint8_t *>(base + 0xEB8);
      reinterpret_cast<void(__thiscall *)(void *, int)>(0x40ecf0)(base + kEntWearObj, head);
    });
    reapply_armor(spawn);
    rcp_guard::run("pc.post", [&] {
      post0 = *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(spawn) + kEntWearObj + kWearEquip + 0x14);
    });
    void *new_actor = nullptr;
    rcp_guard::run("pc.new",
                   [&] { new_actor = *reinterpret_cast<void **>(reinterpret_cast<char *>(spawn) + kEntActor); });
    if (old_actor && new_actor && new_actor != old_actor) {
      void *ov = nullptr, *nv = nullptr;
      rcp_guard::run("pc.vt", [&] {
        ov = *reinterpret_cast<void **>(old_actor);
        nv = *reinterpret_cast<void **>(new_actor);
      });
      if (ov && ov == nv) rcp_guard::run("pc.destroy", [&] { release_actor(old_actor); });
    }
    if (reapply_logged < 40) {  // per-call cap (the shared g_pc_log_count gets eaten by login builds)
      ++reapply_logged;
      rcp_guard::run("pc.log", [&] {
        char *base = reinterpret_cast<char *>(spawn);
        uint32_t t[9];
        for (int s = 0; s < 9; ++s) t[s] = *reinterpret_cast<uint32_t *>(base + kEntEquipSrc + s * 0x14);
        void *def = *reinterpret_cast<void **>(base + 0x1024);
        int def34 = def ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(def) + 0x34) : -1;
        logger::logf("[pcmodel] ensure id=%d race=%d bodytex=%d old=%p new=%p eaf=0x%02x def=%p def34=%d "
                     "chest=%u->%u",
                     id, *reinterpret_cast<int *>(base + kEntRace),
                     (int)*reinterpret_cast<uint8_t *>(base + kEntBodyTex), old_actor, new_actor,
                     *reinterpret_cast<uint8_t *>(base + 0xEAF), def, def34, pre0, post0);
        (void)t;
        (void)wearactor;
      });
    }
  }
  if (!dead.empty()) {
    std::lock_guard<std::mutex> lk(g_mu);
    for (int id : dead) g_spawn_ids.erase(id);
  }
}

}  // namespace

namespace npc_model_settings {

std::vector<ToggleItem> get_items() {
  std::vector<ToggleItem> out;
  std::lock_guard<std::mutex> lk(g_mu);
  for (const auto &r : kNpcRevamps) {
    auto it = g_on.find(r.name);
    out.push_back({r.name, it != g_on.end() && it->second});
  }
  return out;
}

// Toggle a creature to its classic model (on) / modern (off); persists + applies to future builds AND
// live-refreshes every already-visible spawn immediately (no zone) via refresh_world().
void set_on(const std::string &name, bool on) {
  {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!find_revamp(name)) return;
    g_on[name] = on;
    rebuild_redirect();
  }
  persist(name, on);
  refresh_world();  // apply immediately -- rebuild visible spawns, no zone
}

void set_all(bool on) {
  {
    std::lock_guard<std::mutex> lk(g_mu);
    for (const auto &r : kNpcRevamps) g_on[r.name] = on;
    rebuild_redirect();
  }
  for (const auto &r : kNpcRevamps) persist(r.name, on);
  refresh_world();
}

}  // namespace npc_model_settings

NpcModelSwap::NpcModelSwap(RcpService *rcp) : rcp_(rcp) {
  load_settings();  // auto-apply persisted classic-creature toggles ([NpcModels] ini)
  rcp->hooks->Add("rcp_npc_build", static_cast<int>(kBuildActor), BuildActor_hk, hook_type_detour);
  g_build_orig = rcp->hooks->hook_map["rcp_npc_build"]->original(BuildActor_hk);
  rcp->hooks->Add("rcp_npc_find", static_cast<int>(kFindActorDefByName), FindActorDef_hk, hook_type_detour);
  g_find_orig = rcp->hooks->hook_map["rcp_npc_find"]->original(FindActorDef_hk);
  rcp->hooks->Add("rcp_defb_c", static_cast<int>(0x407800), DefBuildClassic_hk, hook_type_detour);
  g_defb_classic = rcp->hooks->hook_map["rcp_defb_c"]->original(DefBuildClassic_hk);
  rcp->hooks->Add("rcp_defb_n", static_cast<int>(0x407dc0), DefBuildNew_hk, hook_type_detour);
  g_defb_new = rcp->hooks->hook_map["rcp_defb_n"]->original(DefBuildNew_hk);
  // temporary learn-hooks on the dress chain (see above)
  rcp->hooks->Add("rcp_learn_apply", static_cast<int>(kApplyArmor), ApplyArmor_hk, hook_type_detour);
  g_apply_orig = rcp->hooks->hook_map["rcp_learn_apply"]->original(ApplyArmor_hk);
  rcp->hooks->Add("rcp_learn_set6", static_cast<int>(0x594de0), Set6_hk, hook_type_detour);
  g_set6_orig = rcp->hooks->hook_map["rcp_learn_set6"]->original(Set6_hk);
  rcp->hooks->Add("rcp_learn_slotref", static_cast<int>(kRefreshEquipSlot), SlotRefresh_hk, hook_type_detour);
  g_slotref_orig = rcp->hooks->hook_map["rcp_learn_slotref"]->original(SlotRefresh_hk);
  logger::logf("[npcbody] BuildActor detour @0x%x + Find @0x%x", (unsigned)kBuildActor,
               (unsigned)kFindActorDefByName);

  // Player-race classic<->Luclin via the native ShouldUseLuclinModel decision (correct armor/face).
  rcp->hooks->Add("rcp_pc_luclin", static_cast<int>(kShouldUseLuclin), ShouldUseLuclin_hk, hook_type_detour);
  g_pc_orig = rcp->hooks->hook_map["rcp_pc_luclin"]->original(ShouldUseLuclin_hk);
  pc_load();
  logger::logf("[pcmodel] ShouldUseLuclin detour @0x%x (%d PC races, ini-ignored)",
               (unsigned)kShouldUseLuclin, (int)(sizeof(kPcRaces) / sizeof(kPcRaces[0])));

  rcp->commands_hook->Add(
      "/rcpbody", {},
      "NPC body-model swap (Phase 2): 'log on|off' logs each spawn's actordef name; '<FROM> <TO>' "
      "redirects a body model, e.g. '/rcpbody SKT RCPSKE' (applied live); 'list'; 'clear'.",
      [](std::vector<std::string> &args) {
        if (args.size() < 2) {
          std::lock_guard<std::mutex> lk(g_mu);
          Rcp::Game::print_chat("rof2ClientPlus body: %d redirect(s), log %s. "
                                "'/rcpbody log on|off', '<FROM> <TO>', 'list', 'clear'.",
                                (int)g_redirect.size(), g_log ? "ON" : "off");
          return true;
        }
        const std::string sub = upper(args[1]);
        if (sub == "LOG") {
          bool on = args.size() >= 3 && (upper(args[2]) == "ON" || args[2] == "1");
          {
            std::lock_guard<std::mutex> lk(g_mu);
            g_log = on;
            g_log_count = 0;
          }
          Rcp::Game::print_chat("rof2ClientPlus body: per-spawn logging %s (zone to generate spawns)",
                                on ? "ON" : "off");
          return true;
        }
        if (sub == "CLEAR") {
          {
            std::lock_guard<std::mutex> lk(g_mu);
            g_manual.clear();
            rebuild_redirect();
          }
          refresh_world();
          Rcp::Game::print_chat("rof2ClientPlus body: manual redirects cleared (catalog toggles kept)");
          return true;
        }
        if (sub == "LIST") {
          Rcp::Game::print_chat("rof2ClientPlus body catalog:");
          for (const auto &it : npc_model_settings::get_items())
            Rcp::Game::print_chat("  %-10s %s", it.name.c_str(), it.on ? "[CLASSIC]" : "modern");
          return true;
        }
        // Catalog toggle: '/rcpbody classic|modern <creature|all>' -- persisted + auto-applied.
        if (sub == "CLASSIC" || sub == "MODERN") {
          const bool on = (sub == "CLASSIC");
          const std::string who = args.size() >= 3 ? upper(args[2]) : "";
          if (who == "ALL") {
            npc_model_settings::set_all(on);
            Rcp::Game::print_chat("rof2ClientPlus body: all creatures -> %s (applied)",
                                  on ? "CLASSIC" : "modern");
            return true;
          }
          for (const auto &r : kNpcRevamps)
            if (!who.empty() && upper(r.name).find(who) != std::string::npos) {
              npc_model_settings::set_on(r.name, on);
              Rcp::Game::print_chat("rof2ClientPlus body: %s -> %s (applied)", r.name,
                                    on ? "CLASSIC" : "modern");
              return true;
            }
          Rcp::Game::print_chat("rof2ClientPlus body: creatures = Skeleton, Wolf, Tiger, Spirit Wolf (or 'all')");
          return true;
        }
        if (args.size() >= 3) {
          std::string from = norm_actordef(args[1]), to = norm_actordef(args[2]);
          {
            std::lock_guard<std::mutex> lk(g_mu);
            g_manual[from] = to;
            rebuild_redirect();
          }
          refresh_world();
          Rcp::Game::print_chat("rof2ClientPlus body: %s -> %s (applied)", from.c_str(), to.c_str());
          return true;
        }
        Rcp::Game::print_chat("rof2ClientPlus body: usage 'log on|off' | '<FROM> <TO>' | 'list' | 'clear'");
        return true;
      });
  logger::log("[npcbody] /rcpbody registered");

  // Player-race classic/Luclin control (native, via the ShouldUseLuclin hook). Applies on the next zone
  // (the client's model config re-consults the hook then); instant re-apply is the follow-up.
  rcp->commands_hook->Add(
      "/rcppc", {},
      "Player-race model: 'classic <race|all>' / 'modern <race|all>' forces the NATIVE classic/Luclin PC "
      "model (armor+face intact), ignoring the eqclient.ini UseLuclin flags; 'list'. Applies on zone.",
      [](std::vector<std::string> &args) {
        auto show = [] {
          Rcp::Game::print_chat("rof2ClientPlus PC models (native; ini ignored; zone to apply):");
          std::lock_guard<std::mutex> lk(g_mu);
          for (const auto &r : kPcRaces)
            Rcp::Game::print_chat("  %-10s %s", r.name, g_pc_classic[r.race] ? "[CLASSIC]" : "modern");
        };
        if (args.size() < 2) {
          show();
          return true;
        }
        const std::string sub = upper(args[1]);
        if (sub == "LIST") {
          show();
          return true;
        }
        if (sub == "NATIVE") {  // DEBUG: disable our PC control entirely -> hook falls through to the ini
          {
            std::lock_guard<std::mutex> lk(g_mu);
            g_pc_classic.clear();
            rebuild_redirect();
          }
          Rcp::Game::print_chat("rof2ClientPlus PC: NATIVE mode (ini-driven, session only)");
          return true;
        }
        if (sub == "CLASSIC" || sub == "MODERN") {
          const bool classic = (sub == "CLASSIC");
          const std::string who = args.size() >= 3 ? upper(args[2]) : "";
          if (who == "ALL") {
            {
              std::lock_guard<std::mutex> lk(g_mu);
              for (const auto &r : kPcRaces) g_pc_classic[r.race] = classic;
            }
            for (const auto &r : kPcRaces) pc_persist(r.name, classic);
            pc_reapply();  // re-run the client's Luclin config with our hook -> (re)loads the right archives
            Rcp::Game::print_chat("rof2ClientPlus PC: all races -> %s (config re-run; zone if not applied)",
                                  classic ? "CLASSIC" : "modern");
            return true;
          }
          if (const PcRace *pr = find_pc_by_name(who)) {
            {
              std::lock_guard<std::mutex> lk(g_mu);
              g_pc_classic[pr->race] = classic;
            }
            pc_persist(pr->name, classic);
            pc_reapply();
            Rcp::Game::print_chat("rof2ClientPlus PC: %s -> %s (config re-run; zone if not applied)", pr->name,
                                  classic ? "CLASSIC" : "modern");
            return true;
          }
          Rcp::Game::print_chat("rof2ClientPlus PC: races = Human, Barbarian, Erudite, Wood Elf, High Elf, "
                                "Dark Elf, Half Elf, Dwarf, Troll, Ogre, Halfling, Gnome (or 'all')");
          return true;
        }
        show();
        return true;
      });
  logger::log("[pcmodel] /rcppc registered");
}
