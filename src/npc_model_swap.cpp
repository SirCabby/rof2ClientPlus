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
#include "font_overlay.h"    // add_scene_draw (self-diag tick rides the proven pre-UI seam)
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

RcpService *g_rcp_svc = nullptr;  // stored by the ctor for lazy hook registration
std::mutex g_mu;
std::map<std::string, std::string> g_redirect;  // active: "<code>_ACTORDEF" -> "<classic>_ACTORDEF"
std::set<std::string> g_persistent_subs;        // redirect TARGETS that must STAY in spawn+0xEBA after the
                                                // build (PC aliases): runtime anim/appearance lookups key
                                                // off the spawn's actordef name, so restoring the native
                                                // code T-poses a new-style alias body. Guarded by g_mu.
int g_pc_log_count = 0;  // cap for PC-race diagnostics ([pcargs]/[pcmodel] lines)
std::map<std::string, bool> g_on;               // catalog display name -> reverted to classic (persisted)
// ELEMENTAL split (the native UseLuclinElementals equivalent, ini-independent). Typed Luclin elementals
// (races 209-212 -> EEL/AEL/WEL/FEL in global5_chr) vs the classic generic ELE (global_chr, tint by body
// texture: 0=earth 1=fire 2=water 3=air, DB-verified). classic: typed -> ELE + the matching texture byte;
// modern: ELE -> typed by its texture byte. Persisted as [NpcModels] "Elemental".
bool g_elem_classic = false;
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

void add_pc_redirects();    // PC-race alias redirects (defined with the PC section below)
void ensure_dagreg_hook();  // lazy DLL attach-slot detour (defined with the PC section below)
// Scoped model-table alias window (defined with the PC section). The table RESTS NATIVE -- leaving an
// alias code (or even a changed flags dword) in a PC record freezes local-player input (probes 1+2,
// 2026-07-16). Dress-time code that must build alias-keyed names (armor materials, face/hair pieces,
// attach dags, def-create) runs between enter/leave, which swap the toggled-modern races' records to
// their alias codes and restore native on exit. Nesting-safe; main-thread only.
void enter_alias_window();
void leave_alias_window();
bool spawn_uses_alias(void *spawn);  // actordef @+0xEBA carries a PC alias prefix (HU7/HU6/...)

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
  g_elem_classic = false;
  for (auto &kv : ini.getSection(kIniSection)) {  // key = creature name, value = 1
    if (std::atoi(kv.second.c_str()) > 0 && find_revamp(kv.first)) g_on[kv.first] = true;
    if (kv.first == "Elemental" && std::atoi(kv.second.c_str()) > 0) g_elem_classic = true;
  }
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
    for (const char *e : {"ELE", "AEL", "EEL", "FEL", "WEL"}) {  // elemental split targets (cheap rebuilds)
      managed.insert(std::string(e) + "_ACTORDEF");
      pc_codes.insert(std::string(e) + "_ACTORDEF");  // cross-code rebuild leaves an orphan actor (proven
                                                      // in-game: elemental toggle ghosted) -> destroy it
    }
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
        rcp_guard::run("npcbody.retint", [&] {  // re-apply the fixed body texture (elemental tint etc.)
          reinterpret_cast<void(__thiscall *)(void *)>(0x594ad0)(spawn);
        });
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
      ensure_dagreg_hook();  // early: DLL is loaded by the time any spawn builds -> catches NATURAL builds
      is_local = spawn == *kLocalPlayer;  // diagnostic only now: with the resting-native table the alias
                                          // redirect is SAFE for the local player's natural builds too
                                          // (and pc_reapply live-rebuilds it with a camera re-acquire)
    });
    if (name[0]) {
      std::string sub;
      int elem_tex = -1;  // >=0: write spawn+0xEB9 (classic ELE tint) when applying an elemental redirect
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
        if (sub.empty()) {  // elemental split: conditional (texture-keyed) redirect, both directions
          std::string u = upper(name);
          if (g_elem_classic) {
            static const struct { const char *from; int tex; } kElemToClassic[] = {
                {"AEL_ACTORDEF", 3}, {"EEL_ACTORDEF", 0}, {"FEL_ACTORDEF", 1}, {"WEL_ACTORDEF", 2}};
            for (const auto &m : kElemToClassic)
              if (u == m.from) {
                sub = "ELE_ACTORDEF";
                elem_tex = m.tex;  // classic ELE tint for this type
              }
          } else if (u == "ELE_ACTORDEF") {
            static const char *kElemByTex[4] = {"EEL_ACTORDEF", "FEL_ACTORDEF", "WEL_ACTORDEF",
                                                "AEL_ACTORDEF"};
            uint8_t bt = 0;
            bt = *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(spawn) + 0xEB9);
            if (bt < 4) sub = kElemByTex[bt];
          }
        }
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
          logger::logf("[pcargs] BUILD-TIME id=%d race=%d local=%d bt=%d ea8=0x%02x ret=%p src=%u,%u,%u,%u,%u,%u,%u,%u,%u wrk=%u,%u,%u,%u,%u,%u,%u,%u,%u",
                       sid, race, (int)is_local, (int)bt, ea8, __builtin_return_address(0), src[0], src[1],
                       src[2], src[3], src[4], src[5], src[6], src[7], src[8], wrk[0], wrk[1], wrk[2],
                       wrk[3], wrk[4], wrk[5], wrk[6], wrk[7], wrk[8]);
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
          if (elem_tex >= 0)  // classic elemental: set the ELE tint for this type (persists -- it IS the tint)
            *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(spawn) + 0xEB9) =
                static_cast<uint8_t>(elem_tex);
          did = true;
        });
        // Alias-keyed dress naming (armor/dag/def) is supplied by the resolver detour; no window here.
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
// MODEL-TABLE record resolution (disasm-corrected 2026-07-15). 0x50f070 is a SINGLETON GETTER for the
// model-table manager @0xDCFCE0 -- it IGNORES its stack args (the old "(buf, race, gender) key builder"
// reading was wrong). 0x50a2d0 = __thiscall(mgr; char *out, int race, int gender): hash-walks the table
// for the record whose +0x20/+0x24 == race/gender, STRCPYs its INLINE code (record+0x8 -- the field every
// armor-material/hair/dag name construction reads) into `out`, and returns a pointer just past that
// inline code's NUL *inside the record* (a miss writes the "HUM"/"HUF" default into `out` and returns an
// unrelated bucket pointer -- the strncmp verify in swap_table_code rejects it). The previous zero-arg
// call worked only off stack residue and ACCESS-VIOLATED when codegen shifted (24x per toggle in the
// 2026-07-15 log), silently leaving the table unswapped.
constexpr uintptr_t kResolveModelCode = 0x50a2d0;  // __thiscall(mgr; char* out, int race, int gender) ret4

int g_table_dumped = 0;  // (retained: referenced by the resolver log cap)
// TEMP freeze-hunt probes (/rcppc probe <n>). The record is no longer mutated at all, so most are inert;
// kept so the existing /rcppc probe handler still compiles.
bool g_probe_skip_code = false;
bool g_probe_skip_flags = false;
bool g_probe_skip_unlink = false;  // don't unlink alias defs from the registry
bool g_probe_skip_table = false;   // skip the toggle-time def invalidation
bool g_probe_skip_storm = false;   // skip the per-spawn rebuild loop
// Registry unlink (proven-safe walk): free an actordef NAME so the next build re-creates its def against
// the current settings.
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

// LEARN-HOOK #3 (temporary): sprintf@0x8dc12f tap, logging every material-name construction ("_MDF") with
// its caller -- answers which code prefix the face/hair texture swap requests for alias actors. cdecl, so
// forwarding a fixed arg window is safe (caller cleans its own pushes).
typedef int(__cdecl *SprintfFn)(char *dst, const char *fmt, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
SprintfFn g_sprintf_orig = nullptr;
int g_mdf_log = 0;
int __cdecl Sprintf_hk(char *dst, const char *fmt, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  int r = g_sprintf_orig(dst, fmt, a, b, c, d);
  if (g_mdf_log < 400 && dst && fmt && dst[0] && dst[1] && dst[2]) {
    // log ONLY alias-prefixed constructions (HU7/HU6/BA7/... = toggle-time activity; login noise excluded)
    static const char *kAliases[] = {"HU7", "HU6", "BA7", "BA6", "ER7", "ER6", "EL7", "EL6",
                                     "HI7", "HI6", "DA7", "DA6", "HA7", "HA6", "DW7", "DW6",
                                     "TR7", "TR6", "OG7", "OG6", "HO7", "HO6", "GN7", "GN6"};
    bool interesting = false;
    for (const char *a : kAliases)
      if (dst[0] == a[0] && dst[1] == a[1] && dst[2] == a[2]) interesting = true;
    if (interesting) {
      ++g_mdf_log;
      logger::logf("[mdf] '%s' (fmt '%s') ret=%p", dst, fmt, __builtin_return_address(0));
    }
  }
  return r;
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
    // hair lead: capture the source object's vtable + the attach-slot API impls (vt[0x19C]=register by dag
    // name, vt[0x1A4]=slot valid?) WHILE a1 is alive -- these are the fns to detour to see why the alias
    // TUNIC probe fails (def+0x34 stays 0 -> no head/hair). Do NOT call them post-build (object transient).
    void *vt = nullptr, *f19c = nullptr, *f1a4 = nullptr;
    rcp_guard::run("defb.vt", [&] {
      void **v = *reinterpret_cast<void ***>(a1);
      vt = v;
      f19c = v[0x19c / 4];
      f1a4 = v[0x1a4 / 4];
    });
    logger::logf("[defb] CLASSIC build def=%p a1=%x a2='%s' vt=%p reg19c=%p chk1a4=%p ret=%p", self, a1, nm,
                 vt, f19c, f1a4, __builtin_return_address(0));
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

// DRESS-CHAIN detours: originally learn-hooks; now ALSO the alias-window carriers -- armor material
// names are built from the model-table code at these entry points, so for alias-bodied spawns the call
// runs inside enter/leave (table shows the alias code only for the duration).
typedef void(__fastcall *Apply1Fn)(void *self, int edx, int slot);
typedef int(__fastcall *Set6Fn)(void *self, int edx, int a1, int a2, int a3, int a4, int a5, int a6);
// TRUE signature of RefreshEquipSlot@0x594e50 (ret 0x20 = 8 stack args), verified from its caller's
// pushes @0x595c9c..0x595cca: (slot, props[5] = the slot's 5-dword ArmorProperties BY VALUE, tintARGB,
// local_only). The old 1-arg detour only survived because gcc emitted the passthrough as a TAIL JUMP
// (caller's frame reached the original untouched); with the real signature it is robust either way.
// (This wrong-arg-count call is also what the old "0x594e50 AV'd when called out of context" note was.)
typedef int(__fastcall *SlotRefreshFn)(void *self, int edx, int slot, uint32_t p0, uint32_t p1,
                                       uint32_t p2, uint32_t p3, uint32_t p4, uint32_t tint, int local);
Apply1Fn g_apply_orig = nullptr;
Set6Fn g_set6_orig = nullptr;
SlotRefreshFn g_slotref_orig = nullptr;
int g_learn_count = 0;
// These dress-chain detours are now LOG-ONLY again: the resolver detour supplies alias names at the
// resolver seam, so no per-call table window is needed here.
//
// EXCEPT ApplyArmorTexture, which also carries the CLASSIC-HELM GATE (2026-07-16). Its WLD head-slot
// path (0x40e656..0x40e6c9) maps helm materials straight to the Luclin 3D helm attach pieces -- material
// 1/2/3 -> IT(5000+mat) plus the 0x40a290 race/gender offset, Velious 240/241 -> IT5028/5029, all living
// in lgequip_amr*.s3d -- gated ONLY on bShowHelm@ActorClient+0x7c. There is NO classic-vs-Luclin body
// check in that mapping: on a stock install classic bodies never show these pieces only because
// lgequip_amr* isn't loaded outside the Luclin config, so the attach lookup misses silently. Our
// GlobalLoad.txt hair fix force-loads the lgequip archives, which un-gated the helm attach and put Luclin
// helms on classic bodies (user report: "modern helm instead of the classic helm"). Gate: when the
// spawn's BUILT body is classic WLD (def+0x34 == 0; the [pcmodel] logs show 0 for every classic def and
// 1 for Luclin), zero bShowHelm for the duration of the call -- the native "helm hidden" branch, which
// both skips the 3D piece and detaches a stale one. Head/coif TEXTURES are untouched (they ride the
// separate set6->0x4099f0 material path), so the classic helm look is exactly stock. Luclin bodies
// (def34=1) keep their 3D helms. The -1 all-slots call recurses per-slot through this same detour; the
// nested calls read saved==0 and pass through, the outermost call restores.
void __fastcall ApplyArmor_hk(void *self, int edx, int slot) {
  if (g_learn_count < 300) {
    ++g_learn_count;
    logger::logf("[learn] apply this=%p slot=%d ret=%p", self, slot, __builtin_return_address(0));
  }
  constexpr int kAcShowHelm = 0x7c;  // ActorClient bShowHelm (eqlib graphics/Actors.h)
  uint8_t *show_helm = nullptr;
  uint8_t saved = 0;
  if (!crash_handler::shutting_down()) {
    rcp_guard::run("pc.helmgate", [&] {
      char *spawn = reinterpret_cast<char *>(self) - 0xea4;    // this = ActorClient, inline @ spawn+0xEA4
      void *def = *reinterpret_cast<void **>(spawn + 0x1024);  // cached CActorDefinition* (kEntDefCache)
      if (def && !*reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(def) + 0x34)) {  // classic WLD body
        show_helm = reinterpret_cast<uint8_t *>(self) + kAcShowHelm;
        saved = *show_helm;
      }
    });
  }
  if (show_helm && saved) {
    *show_helm = 0;
    g_apply_orig(self, edx, slot);
    *show_helm = saved;
    return;
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
int __fastcall SlotRefresh_hk(void *self, int edx, int slot, uint32_t p0, uint32_t p1, uint32_t p2,
                              uint32_t p3, uint32_t p4, uint32_t tint, int local) {
  if (g_learn_count < 300) {
    ++g_learn_count;
    logger::logf("[learn] slotref spawn=%p slot=%d props=%u,%u,%u,%u,%u ret=%p", self, slot, p0, p1, p2,
                 p3, p4, __builtin_return_address(0));
  }
  return g_slotref_orig(self, edx, slot, p0, p1, p2, p3, p4, tint, local);
}
// (The natural spawn-add / illusion visual-init tail @0x595ED0 was briefly detoured to open an alias
//  window around its head/face/hair styling, but its arg count is ambiguous -- callers push 3 vs 4 -- so
//  a fixed-signature detour crashed at char-select. The resolver detour covers its internal name
//  construction, so no hook here is needed.)

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
// Per race AND GENDER (the client's own split: UseLuclin%sMale / UseLuclin%sFemale are the only per-race
// ini literals). Key = (race<<1)|gender, gender 0=male 1=female (kPcRaces codes[] order).
std::map<int, bool> g_pc_classic;  // pc_key -> force classic (true) / force Luclin (false). Persisted.
constexpr char kPcIniSection[] = "PcModels";
int pc_key(int race, int gender) { return (race << 1) | (gender & 1); }
const char *kGenderName[2] = {"Male", "Female"};

const PcRace *find_pc_by_name(const std::string &name) {
  for (const auto &r : kPcRaces)
    if (upper(name) == upper(r.name)) return &r;
  return nullptr;
}
// Parse "<Race>" (gender=-1 -> both) or "<Race> Male"/"<Race> Female".
const PcRace *find_pc_gender(const std::string &name, int *gender) {
  *gender = -1;
  std::string u = upper(name);
  for (const auto &r : kPcRaces) {
    std::string base = upper(r.name);
    if (u == base) return &r;
    for (int g = 0; g < 2; ++g)
      if (u == base + " " + upper(kGenderName[g])) {
        *gender = g;
        return &r;
      }
  }
  return nullptr;
}

// DAG-REGISTER REPAIR SHIM. EQGraphicsDX9.dll RVA 0x41140 = CActor::RegisterAttachSlot(this; slot, dagname)
// (__thiscall ret 8): dag = this->vt[0x1C0]->FindDagByName(name); if (dag) this->0x188[slot] = dag. The
// head/hair attach dies here for alias models: some callers request the dag under the code the SKELETON
// does NOT carry (native vs alias prefix). The shim retries a failed lookup with the 3-char code prefix
// swapped through the PC alias maps (both directions) -- whichever prefix the skeleton actually has wins.
// Registered lazily (the DLL isn't loaded when our ctor runs).
typedef void(__fastcall *DagRegFn)(void *actor, int edx, int slot, char *name);
DagRegFn g_dagreg_orig = nullptr;
bool g_dagreg_hooked = false;
int g_dagreg_log = 0;
void __fastcall DagReg_hk(void *actor, int edx, int slot, char *name) {
  g_dagreg_orig(actor, edx, slot, name);  // the ORIGINAL registration runs first
  if (crash_handler::shutting_down() || !name || slot < 0 || slot > 30) return;
  rcp_guard::run("pc.dagfix", [&] {
    void **slots = reinterpret_cast<void **>(reinterpret_cast<char *>(actor) + 0x188);
    bool orig_ok = slots[slot] != nullptr;  // did the client's own lookup resolve this dag?
    char code[4] = {0};
    for (int i = 0; i < 3 && name[i]; ++i) code[i] = static_cast<char>(std::toupper((unsigned char)name[i]));
    // classify the REQUEST code as a modern-alias code (HU7/HU6/...) vs a native PC code (HUM/HUF/...)
    bool req_alias = false, req_native = false;
    const char *alt = nullptr;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      for (const auto &r : kPcRaces)
        for (int g = 0; g < 2; ++g) {
          if (r.alias[g] && std::strncmp(code, r.codes[g], 3) == 0) {
            req_native = true;
            alt = r.alias[g];
          }
          if (r.alias[g] && std::strncmp(code, r.alias[g], 3) == 0) {
            req_alias = true;
            alt = r.codes[g];
          }
        }
    }
    bool fixed_ok = false;
    if (!orig_ok && alt) {  // retry under the sibling prefix
      char fixed[96];
      std::snprintf(fixed, sizeof(fixed), "%.3s%s", alt, name + 3);
      g_dagreg_orig(actor, edx, slot, fixed);
      fixed_ok = slots[slot] != nullptr;
    }
    // Log EVERY PC (alias OR native) attach request so we can diff modern-vs-classic natural builds. Tagging
    // MODERN (alias code) vs CLASSIC (native code) is the whole point of this pass.
    if ((req_alias || req_native) && g_dagreg_log < 600) {
      ++g_dagreg_log;
      logger::logf("[dagfix] %s actor=%p slot=%d '%s' orig=%s retry=%s", req_alias ? "MODERN" : "classic",
                   actor, slot, name, orig_ok ? "OK" : "MISS",
                   orig_ok ? "-" : (fixed_ok ? "FIXED" : "miss"));
    }
  });
}
void ensure_dagreg_hook() {
  if (g_dagreg_hooked || !g_rcp_svc) return;
  HMODULE dll = GetModuleHandleA("EQGraphicsDX9.dll");
  if (!dll) return;
  uintptr_t addr = reinterpret_cast<uintptr_t>(dll) + 0x41140;
  g_rcp_svc->hooks->Add("rcp_dagreg", static_cast<int>(addr), DagReg_hk, hook_type_detour);
  g_dagreg_orig = g_rcp_svc->hooks->hook_map["rcp_dagreg"]->original(DagReg_hk);
  g_dagreg_hooked = true;
  logger::logf("[dagfix] RegisterAttachSlot detour @%p (dll+0x41140)", (void *)addr);
}


int g_pc_hook_hits = 0;  // DIAGNOSTIC: count/log hook fires so we can see if the config re-runs on zone.
int __fastcall ShouldUseLuclin_hk(void *mgr, int edx, int race, int gender) {
  if (!crash_handler::shutting_down()) {
    bool classic = false, have = false;
    rcp_guard::run("pcluclin.decide", [&] {
      std::lock_guard<std::mutex> lk(g_mu);
      auto it = g_pc_classic.find(pc_key(race, gender));
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

void pc_persist(const std::string &race_name, int gender, bool classic) {
  IO_ini ini(IO_ini::kRcpIniFilename);
  std::string key = race_name + " " + kGenderName[gender & 1];
  if (!classic)
    ini.setValue<int>(kPcIniSection, key, 1);  // store only the non-default (modern/Luclin) overrides
  else
    ini.deleteKey(kPcIniSection, key);
}

void pc_load() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  std::lock_guard<std::mutex> lk(g_mu);
  g_pc_classic.clear();
  if (ini.getValue<int>(kPcIniSection, "Native") > 0) {
    rebuild_redirect();  // DEBUG native mode: empty map -> hook falls through to the eqclient.ini
    return;
  }
  for (const auto &r : kPcRaces)
    for (int g = 0; g < 2; ++g) g_pc_classic[pc_key(r.race, g)] = true;  // default: force classic (ini ignored)
  for (auto &kv : ini.getSection(kPcIniSection)) {
    int g = -1;
    if (const auto *pr = find_pc_gender(kv.first, &g))
      if (std::atoi(kv.second.c_str()) > 0)
        for (int gg = 0; gg < 2; ++gg)
          if (g == -1 || g == gg) g_pc_classic[pc_key(pr->race, gg)] = false;  // persisted modern/Luclin
  }
  rebuild_redirect();  // fold the PC alias redirects into the live map (g_mu held)
}

// PC-race entries for g_redirect (see rebuild_redirect): when a race is toggled OFF its native style,
// redirect each gender's native code to its alias so BuildActor_hk hands the DLL the alias body. The
// swap is transient (spawn+0xEBA restored after the build), so armor/face keying stays native. Races
// without alias archives yet contribute nothing. Caller holds g_mu.
void add_pc_redirects() {
  g_persistent_subs.clear();
  for (const auto &r : kPcRaces)
    for (int g = 0; g < 2; ++g) {
      if (!r.alias[g]) continue;
      g_persistent_subs.insert(std::string(r.alias[g]) + "_ACTORDEF");  // always a persistent sub when active
      auto it = g_pc_classic.find(pc_key(r.race, g));
      if (it == g_pc_classic.end() || it->second) continue;  // native/classic -> no redirect
      g_redirect[std::string(r.codes[g]) + "_ACTORDEF"] = std::string(r.alias[g]) + "_ACTORDEF";
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
// RefreshEquipSlot@0x594e50 = __thiscall(spawn; int slot, ArmorProperties props BY VALUE (5 dwords),
// uint tintARGB, int local_only) ret 0x20 -- THE native per-slot dress. Verified from its spawn-add
// caller @0x595c9c..0x595cca (push 1; push tint; sub esp,0x14 + 5 stores; push slot). For the HEAD
// (slot 0) it routes to SetHead@0x594210 (ret 0x30: new-props[5], old-props[5] = current ActorEquipment[0],
// tint, local_only), which is where BOTH helm looks live: classic WLD (def+0x34==0) -> SetHeldModel(0,"0")
// detach + DLL actor vt[0x14C] SwapHead("%sHE%02d_DMSPRITEDEF") = the classic head-PIECE (coif/kettle);
// Velious 240/241 -> 0x40a5b0 racial IT map + SetHeldModel("IT%d") attach; Luclin (def34) -> HeadType@0xEAB
// + ApplyArmorTexture(0) piece attach. local_only=1 suppresses the WearChange send (0x7994).
constexpr uintptr_t kRefreshEquipSlot = 0x594e50;
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
// ReleaseActor@0x59e3f0 = __thiscall(spawn; CActorInterface* newActor) -- thunks (add ecx,0xEA4;
// jmp 0x40c000) to the ActorClient's SetAttachedActor, which PROPERLY releases the old actor (its own
// vtable teardown, not our hide+scene hack) and writes newActor to ActorClient+0x178 == spawn+0x101C.
// Call with newActor=null to detach+free the live actor and null the slot. This is what the client's own
// illusion/appearance-change path does before a rebuild (site 0x59718b), so it leaves no ghost.
constexpr uintptr_t kReleaseActor = 0x59e3f0;
// PostBuildInit@0x5900e0 = __thiscall(spawn), no args: the world loop runs it right after EnsureActor
// (0x51c772). For the LOCAL/CONTROLLED player (globals 0xDD2630 / 0xDD2644) it re-binds the camera/view to
// the freshly built actor (actor vtable[0x1C](1) @0x590375) -- the exact step our out-of-band rebuild was
// missing, which is why a raw local rebuild left the camera on the old actor. Harmless for other spawns
// (early-returns at the local/controlled check).
constexpr uintptr_t kPostBuildInit = 0x5900e0;
// AttachPlayerCamera@0x507b30 = __thiscall(player), no args: reads the player's CURRENT actor (spawn+0x101C)
// and (re)binds the camera/view to it. This is the per-player camera attach the world-entry local-view
// setup (0x48c8d0) calls; 0x5900e0 only sets up the render anim/pose actor, NOT the camera target -- so
// after an out-of-band rebuild the camera stays on the old actor until THIS runs on the new one.
constexpr uintptr_t kAttachPlayerCamera = 0x507b30;
// LocalViewSetup@0x48c8d0 = __thiscall(rendermgr), no args: the world-entry per-player VIEW/CAMERA setup.
// Operates on BOTH pinstLocalPlayer(0xDD2630) and pinstControlledPlayer(0xDD2644) -- sets up bounding
// (0x58ff80), view state (0x507230), and the camera attach (0x507b30) for each. This is what makes the
// camera work after a zone; the Eye-of-Zomm control-switch reasoning confirms the camera follows the
// CONTROLLED player, and this rebinds it. Call after an out-of-band local rebuild to re-point the camera
// at the new actor.
constexpr uintptr_t kLocalViewSetup = 0x48c8d0;
constexpr int kEntDefCache = 0x1024;  // cached CActorDefinition* -- nulled on a name change so a fresh def
                                      // is resolved for the new code (native path nulls it at 0x5971db)
constexpr int kEntAnimActor = 0x1020;  // render anim/pose actor (from 0x48b270); the camera binds to THIS
                                       // (0x5900e0 @0x590375), not spawn+0x101C
// THE LIVE-SELF CAMERA FIX (2026-07-16). The camera's per-frame anchor is the global pinstViewActor
// @0xD1FD88 (eqlib-named; it sits in the ZERO-INITIALIZED tail of .data, past the 0x401000-0xB78800 image
// scans that "proved" no cached actor pointer existed -- it was there all along). Zone-in acquires it at
// 0x51c829: after the world-entry build loop, the client calls
//   CDisplay::SetViewActor(*(0xDD2660); localPlayer->spawn+0x101C)   [SetViewActor@0x48F030, eqlib-named]
// An out-of-band local rebuild swaps +0x101C but leaves pinstViewActor on the RELEASED old actor, whose
// position never updates again -> camera frozen in world space until a real zone re-runs the acquire
// (exactly the observed symptom; toggling again can't recover because the global stays stale).
// SetViewActor = __thiscall(CDisplay*; CActorInterface*): un-hides the outgoing view actor (vt+0x34
// SetInvisible(false)), stores the new pointer, snaps the DLL camera (CDisplay+0x118) to the new actor's
// position/orientation (vt+0x80/+0x84), and re-resolves the view spawn id. The client's own repair path
// (actor-delete @0x490a10) does the same re-point -- our release_actor hide+unlink hack bypasses it, which
// is why the native "delete" flows never rescued us. ORDER MATTERS: call SetViewActor BEFORE release_actor
// hides the old actor (it would un-hide the ghost if run after).
constexpr uintptr_t kSetViewActor = 0x48F030;
void **const kViewActor = reinterpret_cast<void **>(0xD1FD88);  // pinstViewActor (eqlib)

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

// MODEL-TABLE ALIAS via a RESOLVER-OUTPUT detour (2026-07-16 -- replaces the resting record swap AND the
// scoped-window design). The client's dressing passes (armor materials, head/hair pieces, attach dags,
// def-create names) all build their lookups from the table code by calling the RESOLVER 0x50a2d0, which
// STRCPYs the record's inline code into a caller buffer. We detour that resolver and rewrite the OUTPUT
// buffer's 3-char code to the alias for modern-toggled races -- the RECORD IS NEVER TOUCHED. That matters:
// a resting record edit froze local-player input (probes 1+2), because a per-tick consumer reads the
// record directly; the copied-out string it does NOT read, so rewriting only the copy dresses alias
// bodies everywhere (build/armor/hair/metrics) with zero movement impact and no fragile per-call windows.
// The old visual-init detour (0x595ED0) is GONE -- its arg count is ambiguous (callers push 3 vs 4), so a
// fixed-signature detour stack-imbalanced and crashed at char-select; the resolver covers visual-init's
// internal name construction anyway.
typedef char *(__thiscall *ResolveModelCode2Fn)(void *mgr, char *out, int race, int gender);
ResolveModelCode2Fn g_resolve_orig = nullptr;
int g_resolve_log = 0;
// Keyed off the RESOLVED string (ignore the race/gender args -- for NPCs the 2nd int is a texture, not
// gender, so trusting it would mis-pick male/female). If `out` is a PC race's native code and that exact
// race+gender is toggled modern, overwrite it in place with the same-length alias.
char *__fastcall Resolve_hk(void *mgr, int edx, char *out, int race, int gender) {
  char *r = g_resolve_orig(mgr, out, race, gender);
  if (crash_handler::shutting_down() || !out || !out[0]) return r;
  rcp_guard::run("pc.resolve", [&] {
    if (out[1] == '\0' || out[2] == '\0' || out[3] != '\0') return;  // not a 3-char code
    for (const auto &pr : kPcRaces)
      for (int g = 0; g < 2; ++g) {
        if (!pr.alias[g]) continue;
        if (std::strncmp(out, pr.codes[g], 3) != 0) continue;  // out == this race+gender native code
        bool modern;
        {
          std::lock_guard<std::mutex> lk(g_mu);
          auto it = g_pc_classic.find(pc_key(pr.race, g));
          modern = it != g_pc_classic.end() && !it->second;
        }
        if (modern) {
          std::memcpy(out, pr.alias[g], 3);  // HUM->HU7 etc., same length
          if (g_resolve_log < 30) {
            ++g_resolve_log;
            logger::logf("[resolve] %s -> %s (race=%d)", pr.codes[g], pr.alias[g], pr.race);
          }
        }
        return;
      }
  });
  return r;
}
// Kept as harmless no-ops so existing call sites need no churn (the resolver does the work now; the
// RECORD is never mutated, so there is nothing to enter/leave).
void enter_alias_window() {}
void leave_alias_window() {}
bool spawn_uses_alias(void *) { return false; }

// Toggle-time def invalidation: unlink alias defs from the registry so the next in-window build creates
// them fresh against the current settings (probe 3 proved the unlink itself is input-safe).
void invalidate_alias_defs() {
  for (const auto &r : kPcRaces)
    for (int g = 0; g < 2; ++g) {
      if (!r.alias[g]) continue;
      if (!g_probe_skip_unlink) unlink_actordef_name(std::string(r.alias[g]) + "_ACTORDEF");
    }
}

void pc_reapply() {
  void *mgr = nullptr, *local = nullptr;
  rcp_guard::run("pc.mgr", [&] {
    mgr = *kRenderMgr;
    local = *kLocalPlayer;
  });
  if (!mgr) return;
  ensure_dagreg_hook();  // lazy: the graphics DLL exists by the time anyone toggles
  // 1) Sync the alias redirects to current settings; invalidate stale alias defs. The model table itself
  //    is NOT touched here -- it rests native and flips only inside dress windows (see enter_alias_window).
  {
    std::lock_guard<std::mutex> lk(g_mu);
    rebuild_redirect();
  }
  if (!g_probe_skip_table) invalidate_alias_defs();
  if (g_probe_skip_storm) return;  // probe: settings-only apply, leave every spawn's body as-is
  // 2) Rebuild visible PC-race spawns (BuildActor_hk applies/clears the alias per the map), passing the
  //    spawn's appearance object as p2 (the natural build's parameter). Destroy the leftover actor, then
  //    RE-DRESS the new one (ApplyArmorTexture) -- dressing is a separate pass the rebuild always strips.
  struct PcMatch {
    const PcRace *race;
    int gender;
  };
  std::map<std::string, PcMatch> match;             // native AND alias actordef names -> race+gender
  std::map<std::string, std::string> alias_native;  // alias full name -> native full name (per gender)
  std::vector<int> ids;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    ids.assign(g_spawn_ids.begin(), g_spawn_ids.end());
    // Always visit the local player: its world-entry build can predate the hook's id capture, and the
    // live-self flip must never depend on that bookkeeping.
    if (local) {
      int lid = 0;
      rcp_guard::run("pc.selfid",
                     [&] { lid = *reinterpret_cast<int *>(reinterpret_cast<char *>(local) + kEntSpawnId); });
      if (lid && !g_spawn_ids.count(lid)) ids.push_back(lid);
    }
    for (const auto &r : kPcRaces)
      for (int g = 0; g < 2; ++g) {
        std::string native = std::string(r.codes[g]) + "_ACTORDEF";
        match[native] = {&r, g};
        if (r.alias[g]) {
          std::string alias = std::string(r.alias[g]) + "_ACTORDEF";
          match[alias] = {&r, g};
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
    // LIVE-SELF: the local player rebuilds in place too. The old "camera stays frozen" blocker is solved
    // -- the camera anchors to pinstViewActor@0xD1FD88 (missed by the old pointer scans; see kSetViewActor
    // above), and we re-run the client's own zone-in acquire on the rebuilt actor below.
    const bool is_self = (spawn == local);
    bool mine = false;
    const PcRace *pr = nullptr;
    int spawn_g = 0;
    std::string cur;
    rcp_guard::run("pc.match", [&] {
      char nm[kActorDefCap] = {0};
      read_cstr(reinterpret_cast<char *>(spawn) + kEntActorDef, nm, sizeof(nm));
      cur = upper(nm);
      auto it = match.find(cur);
      if (it != match.end()) {
        mine = true;
        pr = it->second.race;
        spawn_g = it->second.gender;
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
      wanted_classic = g_pc_classic[pc_key(pr->race, spawn_g)];
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
    void *old_actor = nullptr, *old_pose = nullptr;
    rcp_guard::run("pc.detach", [&] {
      char *base = reinterpret_cast<char *>(spawn);
      old_actor = *reinterpret_cast<void **>(base + kEntActor);
      *reinterpret_cast<void **>(base + kEntActor) = nullptr;
      if (is_self) {
        // Mirror the native illusion path (0x5971db): drop the cached def so the build re-resolves the
        // name under the current redirect, and remember the pose actor -- PostBuildInit replaces it.
        old_pose = *reinterpret_cast<void **>(base + kEntAnimActor);
        *reinterpret_cast<void **>(base + kEntDefCache) = nullptr;
      }
    });
    rcp_guard::run("pc.ensure",
                   [&] { reinterpret_cast<void(__thiscall *)(void *)>(kEnsureActor)(spawn); });
    // The world-entry loop's build pair is EnsureActor + PostBuildInit (0x51c76b/0x51c772); the latter
    // rebuilds the render anim/pose actor and runs the local/controlled special-casing. Self only -- the
    // plain rebuild is long proven for everyone else, where PostBuildInit early-outs anyway.
    if (is_self)
      rcp_guard::run("pc.postbuild",
                     [&] { reinterpret_cast<void(__thiscall *)(void *)>(kPostBuildInit)(spawn); });
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
      // NATIVE HEAD-SET (the missing piece of the live-flip dress, 2026-07-16): the helm LOOK is applied
      // by RefreshEquipSlot(0) -> SetHead@0x594210, which the set6 loop above does NOT reach -- set6 only
      // applies slot MATERIALS. On a rebuilt actor the head stays the actordef's default piece (HE00),
      // which the (now-gated) Luclin IT5xxx helm used to mask: "modern helm gone but classic helm missing".
      // Replicate the natural spawn-add call exactly (0x595cca): zero ActorEquipment[0] first so the
      // swap-OUT name is the fresh actor's real default head (fresh spawns start zeroed; Zeal's helm
      // manager documents that a mismatched old-material locks the head), then refresh with the raw
      // Equipment[0] server truth. Classic body -> SwapHead coif/kettle piece (+ Velious racial IT
      // attach); Luclin body -> HeadType + gated piece attach. local_only=1 = no WearChange send.
      std::memset(base + kEntWearObj + kWearEquip, 0, 0x14);
      {
        uint32_t hp[5];
        std::memcpy(hp, base + kEntEquipSrc, sizeof(hp));
        uint32_t htint = *reinterpret_cast<uint32_t *>(base + kEntWearObj + 0x58);
        reinterpret_cast<int(__thiscall *)(void *, int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                           uint32_t, int)>(kRefreshEquipSlot)(spawn, 0, hp[0], hp[1],
                                                                              hp[2], hp[3], hp[4], htint, 1);
      }
      reinterpret_cast<void(__thiscall *)(void *)>(kApplyBodyTexture)(spawn);
      // (Head/hair attach for alias Luclin bodies is an UNSOLVED structural blocker -- see memory. Every
      // live-poke of the head path either did nothing or corrupted classic head state. Removed so classic
      // stays perfect and Luclin renders body+anim+armor. The separate hair MESH is the only gap.)
    });
    reapply_armor(spawn);
    rcp_guard::run("pc.post", [&] {
      post0 = *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(spawn) + kEntWearObj + kWearEquip + 0x14);
    });
    void *new_actor = nullptr;
    rcp_guard::run("pc.new",
                   [&] { new_actor = *reinterpret_cast<void **>(reinterpret_cast<char *>(spawn) + kEntActor); });
    // Self rebuild produced nothing -> put the old actor back rather than leave a detached local player.
    if (is_self && !new_actor && old_actor) {
      rcp_guard::run("pc.selfrestore", [&] {
        *reinterpret_cast<void **>(reinterpret_cast<char *>(spawn) + kEntActor) = old_actor;
      });
      logger::logf("[selfview] rebuild produced no actor -- restored old=%p", old_actor);
      continue;
    }
    // CAMERA RE-ACQUIRE (the previously-missing half of the live-self flip): if the view is anchored to
    // the actor we just replaced, re-run the client's zone-in acquire (0x51c829) on the new one. Must run
    // BEFORE the destroy below -- SetViewActor un-hides the outgoing view actor, which would resurrect
    // the ghost if it ran after release_actor. Gated on "view was ours" so an exotic view (Eye of Zomm
    // style) is left alone, matching how the client only re-points the view it owns.
    if (is_self && new_actor && new_actor != old_actor) {
      rcp_guard::run("pc.viewactor", [&] {
        void *view_before = *kViewActor;
        if (view_before == old_actor || (old_pose && view_before == old_pose)) {
          reinterpret_cast<void(__thiscall *)(void *, void *)>(kSetViewActor)(mgr, new_actor);
          logger::logf("[selfview] view %p -> %p (old=%p pose=%p)", view_before, *kViewActor, old_actor,
                       old_pose);
        } else {
          logger::logf("[selfview] view=%p not on old actor (old=%p pose=%p) -- left alone", view_before,
                       old_actor, old_pose);
        }
      });
    }
    // Destroy the leftover OLD actor (proven hide+scene-unlink); vtable-match guards against UAF.
    if (old_actor && new_actor && new_actor != old_actor) {
      void *ov = nullptr, *nv = nullptr;
      rcp_guard::run("pc.vt", [&] {
        ov = *reinterpret_cast<void **>(old_actor);
        nv = *reinterpret_cast<void **>(new_actor);
      });
      if (ov && ov == nv) rcp_guard::run("pc.destroy", [&] { release_actor(old_actor); });
    }
    // And the leftover old POSE actor (PostBuildInit built a fresh one for the local player); same guard.
    if (is_self && old_pose) {
      void *new_pose = nullptr, *ov = nullptr, *nv = nullptr;
      rcp_guard::run("pc.posevt", [&] {
        new_pose = *reinterpret_cast<void **>(reinterpret_cast<char *>(spawn) + kEntAnimActor);
        if (new_pose && new_pose != old_pose) {
          ov = *reinterpret_cast<void **>(old_pose);
          nv = *reinterpret_cast<void **>(new_pose);
        }
      });
      if (ov && ov == nv) rcp_guard::run("pc.posedestroy", [&] { release_actor(old_pose); });
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

// ---- TEMPORARY diagnostic (modern-self freeze hunt): log the LOCAL player's movement/model state
// every 2s. A frozen-vs-working diff pins which layer dies: input (pos static, run>0, v==0), physics
// (v nonzero or floor/coll odd while pos static), or camera-only (pos MOVES while the screen looks
// frozen). Offsets are eqlib exact-build PlayerBase fields. Read-only; runs on the render thread via
// the shared pre-UI seam (same proven pattern as nameplates). Strip with the other learn-hooks.
void self_diag_tick() {
  static DWORD s_last = 0;
  DWORD now = GetTickCount();
  if (now - s_last < 2000) return;
  s_last = now;
  if (crash_handler::shutting_down()) return;
  rcp_guard::run("pc.selfdiag", [&] {
    char *self = *reinterpret_cast<char **>(kLocalPlayer);
    if (!self) return;
    float py = *reinterpret_cast<float *>(self + 0x64);  // PlayerBase Y/X/Z
    float px = *reinterpret_cast<float *>(self + 0x68);
    float pz = *reinterpret_cast<float *>(self + 0x6c);
    float vy = *reinterpret_cast<float *>(self + 0x70);  // SpeedY/X/Z
    float vx = *reinterpret_cast<float *>(self + 0x74);
    float vz = *reinterpret_cast<float *>(self + 0x78);
    float run = *reinterpret_cast<float *>(self + 0x7c);    // SpeedRun
    float mult = *reinterpret_cast<float *>(self + 0x18);   // SpeedMultiplier
    float hd = *reinterpret_cast<float *>(self + 0x80);     // Heading
    float floorh = *reinterpret_cast<float *>(self + 0x28);  // FloorHeight
    int coll = *reinterpret_cast<int *>(self + 0x24);        // CollidingType
    unsigned pstate = *reinterpret_cast<unsigned *>(self + 0x14c);  // PlayerState (0x20=stunned)
    uint8_t stand = *reinterpret_cast<uint8_t *>(self + 0x35c);     // StandState
    float avh = *reinterpret_cast<float *>(self + 0x138);           // AvatarHeight
    char code[24] = {0};
    read_cstr(self + kEntActorDef, code, sizeof(code));
    void *def = *reinterpret_cast<void **>(self + 0x1024);
    int d34 = def ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(def) + 0x34) : -1;
    float fpc = def ? *reinterpret_cast<float *>(reinterpret_cast<char *>(def) + 0x40) : -99.f;  // FPCOffset
    void *actor = *reinterpret_cast<void **>(self + kEntActor);
    // The render actor's OWN world position (vtable[0x7c] GetPosition(CVector3*)). If the entity p=(...)
    // moves with input but THIS stays frozen, the rebuilt actor is detached from the entity (not the
    // camera); if both move together, the model follows and the camera reads something else.
    float ax = -9999, ay = -9999, az = -9999;
    if (actor) {
      float v3[3] = {0, 0, 0};
      void **avt = *reinterpret_cast<void ***>(actor);
      reinterpret_cast<void(__thiscall *)(void *, float *)>(avt[0x7c / 4])(actor, v3);
      ax = v3[0];
      ay = v3[1];
      az = v3[2];
    }
    void *anim = *reinterpret_cast<void **>(self + kEntAnimActor);
    // pinstControlledPlayer (eqlib 0xDD2644): keyboard/mouse input drives THIS spawn. If it diverges
    // from pinstLocalPlayer, input goes elsewhere = exactly a "can't move or turn" freeze.
    void *controlled = *reinterpret_cast<void **>(0xDD2644);
    float anim_walk = *reinterpret_cast<float *>(self + 0x330);  // walk/run anim speed thresholds (0x592xxx)
    float anim_run = *reinterpret_cast<float *>(self + 0x350);
    // Registry state for the local player's CURRENT actordef name: after a toggle's unlink, a by-name
    // resolve can return a DIFFERENT def object than the spawn's cached one (or null) -- if the frozen
    // state correlates with reg != def, the per-tick gate is a name-resolve identity check.
    void *reg = nullptr;
    if (g_find_orig && code[0]) {
      char full[kActorDefCap];
      std::snprintf(full, sizeof(full), "%s", code);
      reg = g_find_orig(full);
    }
    logger::logf("[selfdiag] p=(%.1f,%.1f,%.1f) actorpos=(%.1f,%.1f,%.1f) hd=%.1f actor=%p anim=%p ctl=%p%s "
                 "def=%p d34=%d code='%s'",
                 px, py, pz, ax, ay, az, hd, actor, anim, controlled,
                 controlled == self ? "(=self)" : "(!SELF)", def, d34, code);
    (void)vx; (void)vy; (void)vz; (void)run; (void)mult; (void)floorh; (void)coll; (void)pstate;
    (void)stand; (void)avh; (void)fpc; (void)anim_walk; (void)anim_run; (void)reg;
  });
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
  out.push_back({"Elemental", g_elem_classic});
  for (const auto &r : kPcRaces)
    for (int g = 0; g < 2; ++g)
      if (r.alias[g])
        out.push_back({std::string(r.name) + " " + kGenderName[g], g_pc_classic[pc_key(r.race, g)]});
  return out;
}

// Toggle a creature to its classic model (on) / modern (off); persists + applies to future builds AND
// live-refreshes every already-visible spawn immediately (no zone) via refresh_world().
void set_on(const std::string &name, bool on) {
  if (name == "Elemental") {
    {
      std::lock_guard<std::mutex> lk(g_mu);
      g_elem_classic = on;
    }
    persist(name, on);
    refresh_world();
    return;
  }
  int g = -1;
  if (const PcRace *pr = find_pc_gender(name, &g)) {  // "<Race> Male"/"<Race> Female" (UI) or "<Race>"
    {
      std::lock_guard<std::mutex> lk(g_mu);
      for (int gg = 0; gg < 2; ++gg)
        if (g == -1 || g == gg) g_pc_classic[pc_key(pr->race, gg)] = on;
    }
    for (int gg = 0; gg < 2; ++gg)
      if (g == -1 || g == gg) pc_persist(pr->name, gg, on);
    pc_reapply();
    return;
  }
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
  g_rcp_svc = rcp;
  load_settings();  // auto-apply persisted classic-creature toggles ([NpcModels] ini)
  rcp->hooks->Add("rcp_npc_build", static_cast<int>(kBuildActor), BuildActor_hk, hook_type_detour);
  g_build_orig = rcp->hooks->hook_map["rcp_npc_build"]->original(BuildActor_hk);
  rcp->hooks->Add("rcp_npc_find", static_cast<int>(kFindActorDefByName), FindActorDef_hk, hook_type_detour);
  g_find_orig = rcp->hooks->hook_map["rcp_npc_find"]->original(FindActorDef_hk);
  rcp->hooks->Add("rcp_mdf", static_cast<int>(0x8dc12f), Sprintf_hk, hook_type_detour);
  g_sprintf_orig = rcp->hooks->hook_map["rcp_mdf"]->original(Sprintf_hk);
  rcp->hooks->Add("rcp_defb_c", static_cast<int>(0x407800), DefBuildClassic_hk, hook_type_detour);
  g_defb_classic = rcp->hooks->hook_map["rcp_defb_c"]->original(DefBuildClassic_hk);
  rcp->hooks->Add("rcp_defb_n", static_cast<int>(0x407dc0), DefBuildNew_hk, hook_type_detour);
  g_defb_new = rcp->hooks->hook_map["rcp_defb_n"]->original(DefBuildNew_hk);
  // dress-chain detours: set6/slotref are temporary learn-hooks; the apply hook ALSO carries the
  // permanent classic-helm gate (suppress Luclin 3D helm pieces on classic WLD bodies -- see ApplyArmor_hk)
  rcp->hooks->Add("rcp_learn_apply", static_cast<int>(kApplyArmor), ApplyArmor_hk, hook_type_detour);
  g_apply_orig = rcp->hooks->hook_map["rcp_learn_apply"]->original(ApplyArmor_hk);
  rcp->hooks->Add("rcp_learn_set6", static_cast<int>(0x594de0), Set6_hk, hook_type_detour);
  g_set6_orig = rcp->hooks->hook_map["rcp_learn_set6"]->original(Set6_hk);
  rcp->hooks->Add("rcp_learn_slotref", static_cast<int>(kRefreshEquipSlot), SlotRefresh_hk, hook_type_detour);
  g_slotref_orig = rcp->hooks->hook_map["rcp_learn_slotref"]->original(SlotRefresh_hk);
  // Resolver detour: rewrites the model-table code STRING (not the record) to the alias for modern races,
  // so all dress-time name construction (body/armor/hair/metrics) resolves alias-keyed with the record
  // resting native (no movement freeze). Replaces the old table-record swap + the crashing visinit hook.
  rcp->hooks->Add("rcp_resolve", static_cast<int>(kResolveModelCode), Resolve_hk, hook_type_detour);
  g_resolve_orig = rcp->hooks->hook_map["rcp_resolve"]->original(Resolve_hk);
  logger::logf("[pcmodel] resolver detour @0x%x", (unsigned)kResolveModelCode);
  logger::logf("[npcbody] BuildActor detour @0x%x + Find @0x%x", (unsigned)kBuildActor,
               (unsigned)kFindActorDefByName);

  // Player-race classic<->Luclin via the native ShouldUseLuclinModel decision (correct armor/face).
  rcp->hooks->Add("rcp_pc_luclin", static_cast<int>(kShouldUseLuclin), ShouldUseLuclin_hk, hook_type_detour);
  g_pc_orig = rcp->hooks->hook_map["rcp_pc_luclin"]->original(ShouldUseLuclin_hk);
  font_overlay::add_scene_draw([](IDirect3DDevice9 *) { self_diag_tick(); });  // TEMP freeze-hunt diag
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

  // Player-race classic/Luclin control (native, via the ShouldUseLuclin hook). Applies INSTANTLY to every
  // visible PC-race spawn including the local player (pc_reapply rebuild + SetViewActor camera re-acquire).
  rcp->commands_hook->Add(
      "/rcppc", {},
      "Player-race model: 'classic <race|all>' / 'modern <race|all>' forces the NATIVE classic/Luclin PC "
      "model (armor+face intact), ignoring the eqclient.ini UseLuclin flags; 'list'. Applies instantly.",
      [](std::vector<std::string> &args) {
        auto show = [] {
          Rcp::Game::print_chat("rof2ClientPlus PC models (ours; ini ignored):");
          std::lock_guard<std::mutex> lk(g_mu);
          for (const auto &r : kPcRaces)
            Rcp::Game::print_chat("  %-10s M:%s F:%s", r.name,
                                  g_pc_classic[pc_key(r.race, 0)] ? "[CLASSIC]" : "modern",
                                  g_pc_classic[pc_key(r.race, 1)] ? "[CLASSIC]" : "modern");
          Rcp::Game::print_chat("  %-10s %s", "Elemental", g_elem_classic ? "[CLASSIC]" : "modern");
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
        if (sub == "PROBE") {  // TEMP freeze bisection: self-resetting modern-human partial applies
          int n = args.size() >= 3 ? std::atoi(args[2].c_str()) : 0;
          auto set_human = [](bool classic) {
            std::lock_guard<std::mutex> lk(g_mu);
            g_pc_classic[pc_key(1, 0)] = classic;  // human m+f (not persisted)
            g_pc_classic[pc_key(1, 1)] = classic;
          };
          // FULL classic reset first so each probe starts from the known-good state.
          g_probe_skip_code = g_probe_skip_flags = g_probe_skip_unlink = false;
          g_probe_skip_table = g_probe_skip_storm = false;
          set_human(true);
          pc_reapply();
          // Then modern with the selected parts disabled.
          const char *desc = "FULL modern (control)";
          switch (n) {
            case 1:  // table CODE swap only -- no flags, no unlink, no spawn rebuilds
              g_probe_skip_flags = g_probe_skip_unlink = g_probe_skip_storm = true;
              desc = "table CODE swap only (no flags/unlink/rebuilds)";
              break;
            case 2:  // table FLAGS only
              g_probe_skip_code = g_probe_skip_unlink = g_probe_skip_storm = true;
              desc = "table FLAGS only (no code/unlink/rebuilds)";
              break;
            case 3:  // registry UNLINK only
              g_probe_skip_code = g_probe_skip_flags = g_probe_skip_storm = true;
              desc = "registry UNLINK only (no code/flags/rebuilds)";
              break;
            case 4:  // spawn REBUILD STORM only (redirect active, table untouched)
              g_probe_skip_table = true;
              desc = "spawn REBUILDS only (table untouched)";
              break;
          }
          set_human(false);
          pc_reapply();
          g_probe_skip_code = g_probe_skip_flags = g_probe_skip_unlink = false;
          g_probe_skip_table = g_probe_skip_storm = false;  // one-shot
          Rcp::Game::print_chat("rof2ClientPlus PC probe %d: classic reset, then %s", n, desc);
          Rcp::Game::print_chat("  -> try to move now; run the next probe directly (no manual reset needed)");
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
          // optional trailing gender: "/rcppc modern human female"; race names may be two words ("Wood Elf")
          int want_g = -1;
          std::vector<std::string> rest(args.begin() + 2, args.end());
          if (!rest.empty()) {
            std::string last = upper(rest.back());
            if (last == "MALE" || last == "FEMALE") {
              want_g = (last == "FEMALE") ? 1 : 0;
              rest.pop_back();
            }
          }
          std::string who;
          for (const auto &w : rest) who += (who.empty() ? "" : " ") + w;
          who = upper(who);
          auto apply_race = [&](const PcRace &r) {
            {
              std::lock_guard<std::mutex> lk(g_mu);
              for (int g = 0; g < 2; ++g)
                if (want_g == -1 || want_g == g) g_pc_classic[pc_key(r.race, g)] = classic;
            }
            for (int g = 0; g < 2; ++g)
              if (want_g == -1 || want_g == g) pc_persist(r.name, g, classic);
          };
          if (who == "ALL") {
            for (const auto &r : kPcRaces) apply_race(r);
            pc_reapply();
            Rcp::Game::print_chat("rof2ClientPlus PC: all races%s -> %s",
                                  want_g == -1 ? "" : (want_g ? " (female)" : " (male)"),
                                  classic ? "CLASSIC" : "modern");
            return true;
          }
          if (who == "ELEMENTAL") {
            npc_model_settings::set_on("Elemental", classic);
            Rcp::Game::print_chat("rof2ClientPlus PC: Elemental -> %s", classic ? "CLASSIC" : "modern");
            return true;
          }
          if (const PcRace *pr = find_pc_by_name(who)) {
            apply_race(*pr);
            pc_reapply();
            Rcp::Game::print_chat("rof2ClientPlus PC: %s%s -> %s", pr->name,
                                  want_g == -1 ? "" : (want_g ? " (female)" : " (male)"),
                                  classic ? "CLASSIC" : "modern");
            return true;
          }
          Rcp::Game::print_chat("rof2ClientPlus PC: races = Human, Barbarian, Erudite, Wood Elf, High Elf, "
                                "Dark Elf, Half Elf, Dwarf, Troll, Ogre, Halfling, Gnome, Elemental (or "
                                "'all'), optionally + male/female");
          return true;
        }
        show();
        return true;
      });
  logger::log("[pcmodel] /rcppc registered");
}
