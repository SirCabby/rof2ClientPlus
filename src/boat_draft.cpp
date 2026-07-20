// rof2ClientPlus - boat draft offset. See boat_draft.h.
#include "boat_draft.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "directx.h"
#include "game_functions.h"  // Rcp::Game::print_chat / is_in_game
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"
#include "string_util.h"

namespace {

// ---- Confirmed RoF2 spawn (PlayerClient) layout, shared with hide_corpse.cpp /
// npc_model_swap.cpp (race offset verified there). ----
void **const kSpawnManager = reinterpret_cast<void **>(0xE641D0);  // pinstSpawnManager.
constexpr int kListFirst = 0x08;   // manager+0x08 -> first spawn in the linked list.
constexpr int kEntNext = 0x08;     // spawn+0x08   -> next spawn.
constexpr int kEntY = 0x64;        // float Y (eqlib PlayerBase; family-confirmed by 0x125/0x148).
constexpr int kEntX = 0x68;        // float X
constexpr int kEntZ = 0x6C;        // float Z
constexpr int kEntName = 0xE4;     // displayed name.
constexpr int kEntType = 0x125;    // uint8: 0=Player, 1=NPC, 2=Corpse.
constexpr int kEntSpawnId = 0x148; // uint32 spawn id.
constexpr int kEntRace = 0xEB4;    // int race (npc_model_swap kEntRace).
constexpr int kEntActor = 0x101C;  // CActorInterface* (render actor wrapper).

inline uint8_t ent_type(char *e) { return *reinterpret_cast<uint8_t *>(e + kEntType); }
inline int ent_race(char *e) { return *reinterpret_cast<int *>(e + kEntRace); }
inline uint32_t ent_spawnid(char *e) { return *reinterpret_cast<uint32_t *>(e + kEntSpawnId); }
inline void *ent_actor(char *e) { return *reinterpret_cast<void **>(e + kEntActor); }
inline float *ent_z(char *e) { return reinterpret_cast<float *>(e + kEntZ); }

// ---- Boat waterline fields (RE'd live on the freporte SirensBane, 2026-07-19,
// via /rcpboat dump + blast). The wrapper actor (spawn+0x101C) caches a position
// triple at +0x2C/+0x30/+0x34 (Y,X,Z) with a copy at +0x114/+0x118/+0x11C -
// writes there stick but do NOT draw. The client writes its computed waterline
// into three DLL-side nodes reached through wrapper pointers; blast-shifting
// them moved the ship on screen (then the sim re-floated them within a frame
// or two, hence the per-frame pre-render re-assert):
//   wrapper+0x028 -> node A: x +0x34, z +0x38
//   wrapper+0x074 -> node B: x +0x68, z +0x6C   (spawn-struct-like layout)
//   wrapper+0x180 -> node C: x +0x40, z +0x44
// Each node's X is checked against the spawn X before its z is touched, so a
// layout mismatch on some other boat/actor type degrades to a skipped write. ----
constexpr int kActorZ = 0x34;
constexpr int kActorZ2 = 0x11C;
struct NodeSpec { int ptr_off, x_off, z_off; };
constexpr NodeSpec kNodes[] = {{0x028, 0x34, 0x38}, {0x074, 0x68, 0x6C}, {0x180, 0x40, 0x44}};

bool looks_like_heap_ptr(uint32_t v) {
  return v >= 0x00010000u && v < 0x7FFF0000u && (v & 3) == 0;
}
bool readable(const void *p, size_t n) { return p && !IsBadReadPtr(p, n); }

// Collect every waterline z field for one boat: wrapper copies + validated
// DLL-side nodes. Returns the count written into zf[] (max 5).
int collect_z_fields(char *boat, float **zf) {
  void *a = ent_actor(boat);
  if (!a) return 0;
  char *ac = static_cast<char *>(a);
  if (!readable(ac, 0x200)) return 0;
  int n = 0;
  zf[n++] = reinterpret_cast<float *>(ac + kActorZ);
  zf[n++] = reinterpret_cast<float *>(ac + kActorZ2);
  const float sx = *reinterpret_cast<float *>(boat + kEntX);
  for (const NodeSpec &ns : kNodes) {
    uint32_t pv = *reinterpret_cast<uint32_t *>(ac + ns.ptr_off);
    if (!looks_like_heap_ptr(pv)) continue;
    char *t = reinterpret_cast<char *>(pv);
    if (!readable(t, static_cast<size_t>(ns.z_off) + 4)) continue;
    float tx = *reinterpret_cast<float *>(t + ns.x_off);
    if (!std::isfinite(tx) || std::fabs(tx - sx) > 50.0f) continue;  // layout sanity: node X must track spawn X.
    zf[n++] = reinterpret_cast<float *>(t + ns.z_off);
  }
  return n;
}

// Boat races: 72 Ship, 73 Launch, 114 Ghost Ship, 141 Boat.
bool is_boat_race(int race) { return race == 72 || race == 73 || race == 114 || race == 141; }

char *first_spawn() {
  void *mgr = *kSpawnManager;
  if (!mgr) return nullptr;
  return static_cast<char *>(*reinterpret_cast<void **>(static_cast<char *>(mgr) + kListFirst));
}
char *next_spawn(char *e) { return static_cast<char *>(*reinterpret_cast<void **>(e + kEntNext)); }

char *first_boat() {
  for (char *e = first_spawn(); e; e = next_spawn(e))
    if (ent_type(e) == 1 && is_boat_race(ent_race(e))) return e;
  return nullptr;
}

// ---- live state ----
float g_draft = 4.0f;  // Units to lower boats. 0 = off. Persisted.
// Per-field bookkeeping: (spawn id, field index) -> the z WE last wrote. If a
// field still holds it, the sim hasn't re-floated since our write - skip. When
// the sim rewrites (fresh waterline incl. bob), apply the draft again. Runs in
// BeginScene, i.e. post-sim / pre-draw, so every rendered frame shows our value.
std::unordered_map<uint64_t, float> g_written;
inline uint64_t fkey(uint32_t id, int idx) { return (static_cast<uint64_t>(id) << 3) | static_cast<unsigned>(idx); }

int g_watch_frames = 0;   // /rcpboat watch: frames left to sample.
int g_frame = 0;

constexpr char kIniSection[] = "Boat";
constexpr char kIniKey[] = "Draft";

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKey)) g_draft = ini.getValue<float>(kIniSection, kIniKey);
}
void save_settings() {
  IO_ini(IO_ini::kRcpIniFilename).setValue<float>(kIniSection, kIniKey, g_draft);
}

// Undo our offset on every tracked field we still own (used when the draft
// changes or turns off, so re-application starts from client truth).
void restore_tracked() {
  for (char *e = first_spawn(); e; e = next_spawn(e)) {
    if (ent_type(e) != 1 || !is_boat_race(ent_race(e))) continue;
    float *zf[5];
    const int n = collect_z_fields(e, zf);
    const uint32_t id = ent_spawnid(e);
    for (int i = 0; i < n; ++i) {
      auto it = g_written.find(fkey(id, i));
      if (it != g_written.end() && std::fabs(*zf[i] - it->second) < 0.01f) *zf[i] = it->second + g_draft;
    }
  }
  g_written.clear();
}

// Diagnostic scan (see /rcpboat dump): wrapper + depth-1 pointees, keyed on the
// boat's X (distinctive) and z.
void log_actor_candidates(char *boat, int span) {
  void *actor = ent_actor(boat);
  if (!actor) {
    logger::logf("[boat] actor is NULL");
    return;
  }
  const float z = *ent_z(boat);
  const float x = *reinterpret_cast<float *>(boat + kEntX);
  const float y = *reinterpret_cast<float *>(boat + kEntY);
  logger::logf("[boat] actor=%p scan (spawn y=%.2f x=%.2f z=%.2f):", actor, (double)y, (double)x, (double)z);
  char *a = static_cast<char *>(actor);
  for (int off = 0; off < span; off += 4) {
    if (!readable(a + off, 4)) break;
    float v = *reinterpret_cast<float *>(a + off);
    if (std::isfinite(v) && (std::fabs(v - z) < 8.0f || std::fabs(v - x) < 8.0f))
      logger::logf("[boat]   actor+0x%03x = %.3f%s", off, (double)v, std::fabs(v - x) < 8.0f ? "  <-- X!" : "");
  }
  for (int off = 0; off < 0x200; off += 4) {
    if (!readable(a + off, 4)) break;
    uint32_t pv = *reinterpret_cast<uint32_t *>(a + off);
    if (!looks_like_heap_ptr(pv) || reinterpret_cast<void *>(pv) == actor) continue;
    char *t = reinterpret_cast<char *>(pv);
    if (!readable(t, 0x200)) continue;
    for (int toff = 0; toff < 0x200; toff += 4) {
      float v = *reinterpret_cast<float *>(t + toff);
      if (std::isfinite(v) && (std::fabs(v - x) < 8.0f || std::fabs(v - z) < 8.0f))
        logger::logf("[boat]   actor+0x%03x -> %p+0x%03x = %.3f%s", off, (void *)t, toff, (double)v,
                     std::fabs(v - x) < 8.0f ? "  <-- X!" : "");
    }
  }
}

// One-shot shotgun probe (kept from the RE phase): subtract dz from EVERY
// z-matching float on the wrapper and its depth-1 pointees.
void blast(char *boat, float dz) {
  void *actor = ent_actor(boat);
  if (!actor) return;
  const float z = *ent_z(boat);
  char *a = static_cast<char *>(actor);
  int hits = 0;
  for (int off = 0; off < 0x400; off += 4) {
    if (!readable(a + off, 4)) break;
    float *vp = reinterpret_cast<float *>(a + off);
    if (std::isfinite(*vp) && std::fabs(*vp - z) < 6.0f) {
      logger::logf("[boat] blast: actor+0x%03x %.3f -> %.3f", off, (double)*vp, (double)(*vp - dz));
      *vp -= dz;
      ++hits;
    }
  }
  for (int off = 0; off < 0x200; off += 4) {
    if (!readable(a + off, 4)) break;
    uint32_t pv = *reinterpret_cast<uint32_t *>(a + off);
    if (!looks_like_heap_ptr(pv) || reinterpret_cast<void *>(pv) == actor) continue;
    char *t = reinterpret_cast<char *>(pv);
    if (!readable(t, 0x200)) continue;
    for (int toff = 0; toff < 0x200; toff += 4) {
      float *vp = reinterpret_cast<float *>(t + toff);
      if (std::isfinite(*vp) && std::fabs(*vp - z) < 6.0f) {
        logger::logf("[boat] blast: actor+0x%03x -> %p+0x%03x %.3f -> %.3f", off, (void *)t, toff, (double)*vp,
                     (double)(*vp - dz));
        *vp -= dz;
        ++hits;
      }
    }
  }
  Rcp::Game::print_chat("rcpboat blast: adjusted %d z-like float(s) by %.1f - did the ship move? (log has offsets)",
                        hits, (double)dz);
}

// Pre-render (BeginScene): the sim has written this frame's boat state (incl.
// the re-float + bob); re-assert the draft on every waterline field before the
// world draws. Change-detected per field, so it never accumulates.
void on_prerender(IDirect3DDevice9 * /*dev*/) {
  if (crash_handler::shutting_down() || !Rcp::Game::is_in_game()) return;

  if (g_watch_frames > 0) {
    --g_watch_frames;
    if ((g_frame++ % 60) == 0) {
      char *b = first_boat();
      if (b) {
        float *zf[5];
        const int n = collect_z_fields(b, zf);
        logger::logf("[boat] watch: '%s' id=%u race=%d spawn_z=%.2f fields=%d", b + kEntName, ent_spawnid(b),
                     ent_race(b), (double)*ent_z(b), n);
        for (int i = 0; i < n; ++i) logger::logf("[boat]   field[%d] = %.3f", i, (double)*zf[i]);
      } else {
        logger::logf("[boat] watch: no boat-race spawn in zone");
      }
    }
  }

  if (g_draft == 0.0f) return;
  for (char *e = first_spawn(); e; e = next_spawn(e)) {
    if (ent_type(e) != 1 || !is_boat_race(ent_race(e))) continue;
    float *zf[5];
    const int n = collect_z_fields(e, zf);
    const uint32_t id = ent_spawnid(e);
    for (int i = 0; i < n; ++i) {
      auto it = g_written.find(fkey(id, i));
      if (it != g_written.end() && std::fabs(*zf[i] - it->second) < 0.001f) continue;  // still ours.
      const float nz = *zf[i] - g_draft;  // sim wrote a fresh waterline - offset it.
      *zf[i] = nz;
      g_written[fkey(id, i)] = nz;
    }
  }
}

void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus /rcpboat: draft=%.1f (boats lowered by this many units; 0=off).", (double)g_draft);
  int n = 0;
  for (char *e = first_spawn(); e; e = next_spawn(e)) {
    if (ent_type(e) != 1 || !is_boat_race(ent_race(e))) continue;
    float *zf[5];
    const int k = collect_z_fields(e, zf);
    char fields[128];
    int len = 0;
    for (int i = 0; i < k && len < 100; ++i)
      len += snprintf(fields + len, sizeof(fields) - len, "%s%.1f", i ? "/" : "", (double)*zf[i]);
    Rcp::Game::print_chat("  boat: '%s' id=%u race=%d spawn_z=%.2f z-fields[%d]=%s", e + kEntName, ent_spawnid(e),
                          ent_race(e), (double)*ent_z(e), k, fields);
    ++n;
  }
  if (!n) Rcp::Game::print_chat("  (no boat-race spawns in this zone)");
}

}  // namespace

namespace boat_draft_settings {
float get_draft() { return g_draft; }
void set_draft(float units) {
  restore_tracked();  // undo the old offset first so the new one applies to client truth.
  g_draft = units;
  save_settings();
}
}  // namespace boat_draft_settings

BoatDraft::BoatDraft(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  directx::add_prerender_callback(on_prerender);

  rcp->commands_hook->Add(
      "/rcpboat", {},
      "Boat draft offset: '/rcpboat draft <units>' lowers boats (default 4, 0=off). Probes: status (no arg), "
      "watch, dump, blast <dz>, setz <z>, seta <off_hex> <float>.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (Rcp::String::compare_insensitive(a, "draft") && args.size() >= 3) {
            boat_draft_settings::set_draft(static_cast<float>(atof(args[2].c_str())));
            print_status();
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "watch")) {  // ~6s of once-per-second samples to the log.
            g_watch_frames = 360;
            Rcp::Game::print_chat("rcpboat: watching the first boat for ~6s -> rof2ClientPlus.log");
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "dump")) {  // one-shot candidate scan.
            char *b = first_boat();
            if (!b) {
              Rcp::Game::print_chat("rcpboat: no boat-race spawn in this zone.");
              return true;
            }
            logger::logf("[boat] dump: '%s' id=%u race=%d z=%.2f", b + kEntName, ent_spawnid(b), ent_race(b),
                         (double)*ent_z(b));
            log_actor_candidates(b, 0x300);
            Rcp::Game::print_chat("rcpboat: dump written to rof2ClientPlus.log (z=%.2f actor=%p).",
                                  (double)*ent_z(b), ent_actor(b));
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "blast") && args.size() >= 3) {  // shotgun z-probe (one-shot).
            char *b = first_boat();
            if (!b) {
              Rcp::Game::print_chat("rcpboat: no boat-race spawn in this zone.");
              return true;
            }
            blast(b, static_cast<float>(atof(args[2].c_str())));
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "setz") && args.size() >= 3) {  // one-shot spawn-z poke.
            char *b = first_boat();
            if (!b) {
              Rcp::Game::print_chat("rcpboat: no boat-race spawn in this zone.");
              return true;
            }
            float old = *ent_z(b);
            *ent_z(b) = static_cast<float>(atof(args[2].c_str()));
            Rcp::Game::print_chat("rcpboat: spawn z %.2f -> %.2f ('%s').", (double)old, (double)*ent_z(b),
                                  b + kEntName);
            return true;
          }
          if (Rcp::String::compare_insensitive(a, "seta") && args.size() >= 4) {  // one-shot actor float poke.
            char *b = first_boat();
            if (!b || !ent_actor(b)) {
              Rcp::Game::print_chat("rcpboat: no boat (or it has no actor).");
              return true;
            }
            const int off = static_cast<int>(strtol(args[2].c_str(), nullptr, 16));
            float *p = reinterpret_cast<float *>(static_cast<char *>(ent_actor(b)) + off);
            float old = *p;
            *p = static_cast<float>(atof(args[3].c_str()));
            Rcp::Game::print_chat("rcpboat: actor+0x%x  %.3f -> %.3f", off, (double)old, (double)*p);
            return true;
          }
        }
        print_status();
        return true;
      });
  logger::logf("[boat] loaded: draft=%.2f (/rcpboat registered, pre-render re-assert)", (double)g_draft);
}

BoatDraft::~BoatDraft() {}
