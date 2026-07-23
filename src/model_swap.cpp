// rof2ClientPlus - live classic/new held-model toggle. See model_swap.h.
#include "model_swap.h"
#include "rebase.h"

#include <windows.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"  // Rcp::Game::print_chat
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// SetHeldModel(this=spawn, slot, char* tag, a2, a3): held-item attach primitive. ret 0x10.
// slot 7/8 = primary/secondary hand; arg1 (`tag`) is the model-name string ("IT7", ...).
const uintptr_t kSetHeldModel = ::Rcp::eqva(0x5923f0);
const uintptr_t kSelfPtr = ::Rcp::eqva(0xdd2630);  // *(void**)kSelfPtr = local player spawn
// Resolve a spawn id -> its LIVE eqlib spawn ptr via the client's own spawn manager (mirrors
// floating_damage, confirmed in-game). Held models attach to eqlib PlayerClient objects, which are
// NOT the game_structures Entity list get_entity_list() returns -- so we key spawns by id and re-
// resolve. eqlib PlayerBase: spawn id @0x148; PlayerManagerClient::GetSpawnByID(int)@0x5996E0.
constexpr int kEntSpawnId = 0x148;
void **const kSpawnManager = reinterpret_cast<void **>(::Rcp::eqva(0xE641D0));
const uintptr_t kGetSpawnByID = ::Rcp::eqva(0x5996E0);
constexpr char kIniSection[] = "Models";
constexpr char kClassicPrefix[] = "RCP";  // tools/isolate_archive.py prefix; RCPIT<n>_ACTORDEF

// The revamp catalog: held-item models that have BOTH a classic and a modern version. Setting one
// "classic" redirects its modern tag IT<modern> to RCPIT<classic>. Two groups:
//   * same-number (modern==classic): the classic gequip model was overwritten in place by
//     equipment-01.eqg; rendering RCPIT<n> restores it.
//   * renumbered (modern!=classic): the item's model was re-pointed to a new high IT; the classic
//     lives at a low IT. This map was derived EXACTLY by diffing the TAKP (2002) item DB against
//     the modern DB (tools/... ; see model-swap notes). New-only weapons are absent by construction.
struct Revamp {
  int modern;
  int classic;
  const char *name;
};
const Revamp kRevamps[] = {
    // -- same-number overrides (equipment-01.eqg) --
    {7, 7, "Mace"},        {14, 14, "Warhammer"}, {15, 15, "Horn"},   {18, 18, "Club"},
    {23, 23, "Halberd"},   {24, 24, "Morning Star"}, {30, 30, "Fork"}, {31, 31, "Warclub"},
    {32, 32, "Broom"},     {39, 39, "Scythe"},    {40, 40, "Sickle"}, {41, 41, "Scimitar"},
    {42, 42, "Blade"},     {43, 43, "Pick"},      {52, 52, "Shovel"}, {58, 58, "Bastard Sword"},
    {60, 60, "Battle Axe"}, {61, 61, "Whip"},     {67, 67, "Totem"},  {79, 79, "Trident"},
    {81, 81, "Staff"},
    // -- renumbered (TAKP-diff derived) --
    {10649, 1, "Sword"},   {10652, 26, "Sword"},  {10653, 34, "Sword"},   {10686, 59, "Sword"},
    {10728, 3, "Sword"},   {10648, 2, "2H Sword"}, {10613, 17, "Dagger"}, {10650, 5, "Dagger"},
    {10007, 37, "Mace"},   {10608, 19, "Mace"},   {10638, 33, "Mace"},    {10614, 4, "Bow"},
    {10695, 25, "Idol"},   {10697, 67, "Idol"},   {11057, 400, "Idol"},   {10645, 27, "Book"},
    {10646, 28, "Book"},   {11060, 403, "Book"},  {11068, 400, "Book"},   {11055, 64, "Weapon"},
    {11066, 403, "Weapon"}, {10611, 403, "Throwing"}, {10725, 5, "Throwing"}, {10601, 6, "Instrument"},
    {10603, 21, "Instrument"}, {10100, 16, "Spear"},
};

typedef int(__fastcall *SetHeldFn)(void *self, int edx, int slot, void *tag, int a2, int a3);
SetHeldFn g_orig_held = nullptr;
bool g_early_installed = false;  // install_early ran at DllMain; the ctor must not double-detour

std::mutex g_mu;
std::map<int, int> g_map;                        // applied redirects: modern IT -> classic IT
std::map<std::string, std::string> g_redirect;   // "IT<m>"->"RCPIT<c>", derived from g_map
std::string g_last_main, g_last_off;             // self's last-seen ORIGINAL mainhand/offhand tag
// Per-spawn last-seen ORIGINAL weapon tags {mainhand, offhand}, keyed by SPAWN ID. Lets a toggle
// re-attach the REAL weapon of every spawn (via GetSpawnByID), not just self, using the exact tag the
// client resolved. Ids that no longer resolve (despawned) are pruned on refresh.
std::map<int, std::pair<std::string, std::string>> g_spawn_tag;
constexpr size_t kMaxSpawnTags = 4096;

const Revamp *find_revamp(int modern) {
  for (const auto &r : kRevamps)
    if (r.modern == modern) return &r;
  return nullptr;
}

std::string upper(std::string s) {
  for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

int it_num(const std::string &s) {  // "IT10648" or "10648" -> 10648, else -1
  std::string u = upper(s);
  const char *p = u.c_str();
  if (u.rfind("IT", 0) == 0) p += 2;
  if (!*p || !std::isdigit(static_cast<unsigned char>(*p))) return -1;
  return std::atoi(p);
}

void rebuild_redirect() {  // caller holds g_mu
  g_redirect.clear();
  for (auto &kv : g_map)
    g_redirect["IT" + std::to_string(kv.first)] =
        std::string(kClassicPrefix) + "IT" + std::to_string(kv.second);
}

void read_tag(void *a1, char out[24]) {
  out[0] = '\0';
  const char *p = reinterpret_cast<const char *>(a1);
  if (!p) return;
  int i = 0;
  for (; i < 23 && p[i]; ++i) out[i] = (p[i] >= 32 && p[i] < 127) ? p[i] : '?';
  out[i] = '\0';
}

// Spawn id of the object SetHeldModel operates on (eqlib PlayerBase layout). Caller guards the deref.
int read_spawn_id(void *spawn) {
  if (!spawn) return 0;
  return static_cast<int>(*reinterpret_cast<uint32_t *>(static_cast<char *>(spawn) + kEntSpawnId));
}

// The client's own id->live-spawn lookup; returns nullptr if the id no longer exists.
void *get_spawn_by_id(int id) {
  void *mgr = *kSpawnManager;
  if (!mgr || id <= 0) return nullptr;
  return reinterpret_cast<void *(__thiscall *)(void *, int)>(kGetSpawnByID)(mgr, id);
}

// Detour: on weapon slots, remember self's hands and, if the tag has a classic redirect, hand the
// original a substituted tag (thread-local buffer that outlives the synchronous call).
int __fastcall SetHeld_hk(void *self, int edx, int slot, void *tag, int a2, int a3) {
  if ((slot == 7 || slot == 8) && !crash_handler::shutting_down()) {
    char name[24];
    bool is_self = false;
    int sid = 0;
    rcp_guard::run("modelswap.tag", [&] {
      is_self = (self == *reinterpret_cast<void **>(kSelfPtr));
      read_tag(tag, name);
      sid = read_spawn_id(self);
    });
    if (name[0]) {
      std::string redir;
      {
        std::lock_guard<std::mutex> lk(g_mu);
        if (is_self) (slot == 7 ? g_last_main : g_last_off) = name;
        if (sid > 0 && (g_spawn_tag.size() < kMaxSpawnTags || g_spawn_tag.count(sid))) {
          auto &hands = g_spawn_tag[sid];
          (slot == 7 ? hands.first : hands.second) = name;
        }
        auto it = g_redirect.find(upper(name));
        if (it != g_redirect.end()) redir = it->second;
      }
      if (!redir.empty()) {
        static thread_local char sub[24];
        std::snprintf(sub, sizeof(sub), "%s", redir.c_str());
        return g_orig_held(self, edx, slot, sub, a2, a3);
      }
    }
  }
  return g_orig_held(self, edx, slot, tag, a2, a3);
}

void reattach(void *self, int slot, const std::string &orig) {
  if (orig.empty()) return;
  std::string fin = orig;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_redirect.find(upper(orig));
    if (it != g_redirect.end()) fin = it->second;
  }
  char b[24];
  std::snprintf(b, sizeof(b), "%s", fin.c_str());
  g_orig_held(self, 0, slot, b, 0, 1);
}

// Re-attach the held weapons of EVERY spawn in the zone (self, players, NPCs, pets) that is holding
// a model in our catalog, so a toggle applies LIVE without waiting for the spawn to zone/re-equip.
// Each spawn's real weapon tags come from g_spawn_tag (populated by the detour as the client attaches
// them) -- not from a struct field -- so we re-attach the exact tag the client resolved. reattach()
// then resolves the redirect: classic if the model is set to classic, else the modern tag (which
// reverts a previously-classic spawn back to modern). We only touch spawns that are BOTH currently in
// the live entity list (no stale pointers) AND holding a catalog weapon. Targets are collected first;
// SetHeldModel must not run mid-walk. Stale cache entries are pruned. Main-thread only (command/UI).
void refresh_world() {
  // 1. SELF: proven direct path. *(void**)kSelfPtr is exactly the `this` the detour cached self's
  // tags under, so this always works.
  void *self = nullptr;
  int self_id = 0;
  rcp_guard::run("modelswap.self", [&] {
    self = *reinterpret_cast<void **>(kSelfPtr);
    self_id = read_spawn_id(self);
  });
  std::string sm, so;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    sm = g_last_main;
    so = g_last_off;
  }
  if (self) {
    reattach(self, 7, sm);
    reattach(self, 8, so);
  }

  // 2. OTHER SPAWNS: re-resolve each cached spawn id to its LIVE ptr via the client's spawn manager
  // (null => despawned, pruned). This is the same eqlib ptr SetHeldModel expects, so re-attaching it
  // applies the redirect just like it did for self.
  std::map<int, std::pair<std::string, std::string>> snap;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    snap = g_spawn_tag;
  }
  struct Job {
    void *ent;
    int slot;
    std::string tag;
  };
  std::vector<Job> jobs;
  std::vector<int> dead;
  for (auto &kv : snap) {
    if (kv.first == self_id) continue;  // self handled above
    void *ent = nullptr;
    rcp_guard::run("modelswap.byid", [&] { ent = get_spawn_by_id(kv.first); });
    if (!ent) {
      dead.push_back(kv.first);
      continue;
    }
    const std::string &m = kv.second.first;
    const std::string &o = kv.second.second;
    if (!m.empty() && find_revamp(it_num(m))) jobs.push_back({ent, 7, m});
    if (!o.empty() && find_revamp(it_num(o))) jobs.push_back({ent, 8, o});
  }
  for (auto &j : jobs) reattach(j.ent, j.slot, j.tag);
  {
    std::lock_guard<std::mutex> lk(g_mu);
    for (int id : dead) g_spawn_tag.erase(id);
  }
  logger::logf("[modelswap] refresh_world self=%p self_id=%d mainTag='%s' cached=%d jobs=%d dead=%d",
               self, self_id, sm.c_str(), (int)snap.size(), (int)jobs.size(), (int)dead.size());
}

void persist(int modern, int classic) {  // classic <= 0 removes
  IO_ini ini(IO_ini::kRcpIniFilename);
  std::string key = "IT" + std::to_string(modern);
  if (classic > 0)
    ini.setValue<int>(kIniSection, key, classic);
  else
    ini.deleteKey(kIniSection, key);
}

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  std::lock_guard<std::mutex> lk(g_mu);
  g_map.clear();
  for (auto &kv : ini.getSection(kIniSection)) {  // key="IT10648", value="2"
    int modern = it_num(kv.first), classic = std::atoi(kv.second.c_str());
    if (modern > 0 && classic > 0) g_map[modern] = classic;
  }
  rebuild_redirect();
}

// modern<=0 removes; else map IT<modern> -> RCPIT<classic>. Persists + applies live.
void set_map(int modern, int classic) {
  if (modern <= 0) return;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    if (classic > 0)
      g_map[modern] = classic;
    else
      g_map.erase(modern);
    rebuild_redirect();
  }
  persist(modern, classic);
  refresh_world();
}

int resolve_arg(const std::string &a) {  // "<name|IT#|#>" -> a catalog modern IT, or -1
  int n = it_num(a);
  if (n > 0 && find_revamp(n)) return n;
  std::string u = upper(a);
  for (const auto &r : kRevamps)
    if (upper(r.name).find(u) != std::string::npos) return r.modern;
  return -1;
}

bool parse_state(const std::string &s, bool *out) {
  std::string u = upper(s);
  if (u == "CLASSIC" || u == "OLD" || u == "1" || u == "ON") { *out = true; return true; }
  if (u == "NEW" || u == "MODERN" || u == "0" || u == "OFF") { *out = false; return true; }
  return false;
}

}  // namespace

namespace model_settings {

std::vector<ToggleItem> get_items() {
  std::vector<ToggleItem> out;
  std::lock_guard<std::mutex> lk(g_mu);
  for (const auto &r : kRevamps)
    out.push_back({r.modern, r.classic, r.name, g_map.count(r.modern) > 0});
  return out;
}

void set_on(int modern, bool on) {
  const Revamp *r = find_revamp(modern);
  if (!r) return;
  set_map(modern, on ? r->classic : -1);
}

void set_all(bool on) {
  for (const auto &r : kRevamps) {
    {
      std::lock_guard<std::mutex> lk(g_mu);
      if (on)
        g_map[r.modern] = r.classic;
      else
        g_map.erase(r.modern);
    }
    persist(r.modern, on ? r.classic : -1);
  }
  {
    std::lock_guard<std::mutex> lk(g_mu);
    rebuild_redirect();
  }
  refresh_world();
}

}  // namespace model_settings

namespace model_swap_api {

// Armed at DllMain so the char-select preview's held-weapon attaches (built during client init,
// before the first ProcessGameEvents tick) take the classic redirect. Everything the detour touches
// is attach-safe: logger/crash_handler/rcp_guard install earlier in on_attach, and the redirect map
// is plain file-static state loaded from the ini right here. The ctor skips its own Add when this ran.
void install_early(HookWrapper *hooks) {
  if (g_early_installed) return;
  load_settings();
  hooks->Add("rcp_model_held", static_cast<int>(kSetHeldModel), SetHeld_hk, hook_type_detour);
  g_orig_held = hooks->hook_map["rcp_model_held"]->original(SetHeld_hk);
  g_early_installed = true;
  logger::logf("[modelswap] EARLY SetHeldModel detour @0x%x (attach; %d models classic)",
               (unsigned)kSetHeldModel, (int)g_map.size());
}

// Re-attach one spawn's cached held-model tags (slots 7/8) onto its CURRENT actor. Used by the PC/NPC
// body swap right after it rebuilds an actor: the rebuild strips every held attachment, and its wear
// redress (armor materials / head) never reaches the hand slots. The tags are the exact strings the
// client last passed to SetHeldModel for this spawn (detour cache -- same source refresh_world uses,
// proven in-game), and reattach() re-applies any classic weapon redirect. Spawns with no cached tags
// (never held anything) no-op.
void reattach_held(void *spawn) {
  if (!spawn || !g_orig_held || crash_handler::shutting_down()) return;
  bool is_self = false;
  int sid = 0;
  rcp_guard::run("modelswap.reheld", [&] {
    is_self = (spawn == *reinterpret_cast<void **>(kSelfPtr));
    sid = read_spawn_id(spawn);
  });
  std::string m, o;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    if (is_self) {
      m = g_last_main;
      o = g_last_off;
    }
    auto it = g_spawn_tag.find(sid);
    if (it != g_spawn_tag.end()) {
      if (m.empty()) m = it->second.first;
      if (o.empty()) o = it->second.second;
    }
  }
  if (m.empty() && o.empty()) return;
  reattach(spawn, 7, m);
  reattach(spawn, 8, o);
  static int s_log = 0;  // diagnostic for the in-game confirm pass; strip with the other learn logs
  if (s_log < 60) {
    ++s_log;
    logger::logf("[modelswap] reattach_held spawn=%p id=%d self=%d main='%s' off='%s'", spawn, sid,
                 (int)is_self, m.c_str(), o.c_str());
  }
}

}  // namespace model_swap_api

ModelSwap::ModelSwap(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  if (!g_early_installed) {  // normally armed at DllMain (install_early) so char select is covered
    rcp->hooks->Add("rcp_model_held", static_cast<int>(kSetHeldModel), SetHeld_hk, hook_type_detour);
    g_orig_held = rcp->hooks->hook_map["rcp_model_held"]->original(SetHeld_hk);
  }
  logger::logf("[modelswap] SetHeldModel detour @0x%x%s; %d/%d models set classic",
               (unsigned)kSetHeldModel, g_early_installed ? " (attach)" : "", (int)g_map.size(),
               (int)(sizeof(kRevamps) / sizeof(kRevamps[0])));

  rcp->commands_hook->Add(
      "/rcpmodels", {"/rcpmodelswap"},
      "Revert revamped held models to their classic look: '/rcpmodels' status; 'list' shows the "
      "catalog; 'all classic|new'; '<name|IT#> classic|new' one; 'this <classicIT#>' maps the "
      "weapon in your hand (custom).",
      [](std::vector<std::string> &args) {
        const int total = sizeof(kRevamps) / sizeof(kRevamps[0]);
        if (args.size() < 2) {
          size_t on;
          { std::lock_guard<std::mutex> lk(g_mu); on = g_map.size(); }
          Rcp::Game::print_chat("rof2ClientPlus models: %zu/%d reverted to classic. "
                                "'/rcpmodels list', 'all classic|new', '<name> classic|new'.",
                                on, total);
          return true;
        }
        const std::string sub = upper(args[1]);

        if (sub == "LIST") {
          for (const auto &it : model_settings::get_items())
            Rcp::Game::print_chat("  %-13s IT%-6d-> IT%-4d %s", it.name.c_str(), it.modern,
                                  it.classic, it.on ? "[CLASSIC]" : "new");
          return true;
        }
        if (sub == "THIS") {
          int modern;
          { std::lock_guard<std::mutex> lk(g_mu); modern = it_num(g_last_main); }
          if (modern <= 0) {
            Rcp::Game::print_chat("rof2ClientPlus: no weapon in your mainhand to map.");
            return true;
          }
          if (args.size() >= 3 && upper(args[2]) != "OFF") {
            int classic = it_num(args[2]);
            if (classic <= 0) classic = resolve_arg(args[2]);
            if (classic <= 0) { Rcp::Game::print_chat("rof2ClientPlus: give a classic IT#, e.g. 'this IT53'."); return true; }
            set_map(modern, classic);
            Rcp::Game::print_chat("rof2ClientPlus: mainhand IT%d -> classic IT%d", modern, classic);
          } else {
            set_map(modern, -1);
            Rcp::Game::print_chat("rof2ClientPlus: mainhand IT%d -> new", modern);
          }
          return true;
        }
        if (sub == "MAP" && args.size() >= 3) {
          int modern = it_num(args[2]);
          int classic = (args.size() >= 4 && upper(args[3]) != "OFF") ? it_num(args[3]) : -1;
          if (modern <= 0) { Rcp::Game::print_chat("rof2ClientPlus: 'map IT<modern> IT<classic>|off'"); return true; }
          set_map(modern, classic);
          Rcp::Game::print_chat("rof2ClientPlus: IT%d -> %s", modern,
                                classic > 0 ? ("classic IT" + std::to_string(classic)).c_str() : "new");
          return true;
        }

        bool st;
        if (sub == "ALL" && args.size() >= 3 && parse_state(args[2], &st)) {
          model_settings::set_all(st);
          Rcp::Game::print_chat("rof2ClientPlus: all %d revamped models -> %s", total,
                                st ? "CLASSIC" : "new");
          return true;
        }
        // Bare state word -- '/rcpmodels classic' | 'new' -- is shorthand for applying to ALL.
        if (args.size() == 2 && parse_state(args[1], &st)) {
          model_settings::set_all(st);
          Rcp::Game::print_chat("rof2ClientPlus: all %d revamped models -> %s", total,
                                st ? "CLASSIC" : "new");
          return true;
        }
        int modern = resolve_arg(args[1]);
        if (modern < 0) {
          Rcp::Game::print_chat("rof2ClientPlus: unknown '%s' ('/rcpmodels list')", args[1].c_str());
          return true;
        }
        if (args.size() < 3 || !parse_state(args[2], &st)) {
          std::lock_guard<std::mutex> lk(g_mu);
          st = g_map.count(modern) == 0;
        }
        model_settings::set_on(modern, st);
        const Revamp *r = find_revamp(modern);
        Rcp::Game::print_chat("rof2ClientPlus: %s (IT%d) -> %s", r ? r->name : "?", modern,
                              st ? "CLASSIC" : "new");
        return true;
      });
  logger::log("[modelswap] /rcpmodels registered");
}
