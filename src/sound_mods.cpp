// rof2ClientPlus - disable / adjust game sounds by name. See sound_mods.h.
#include "sound_mods.h"
#include "rebase.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
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

// Asset::Play @ 0x5BFBD0 - __thiscall(this=audio asset, void* param), ret 4. The single choke every
// sound start funnels through (its main caller, the "play sound at index N" helper 0x4A9140, is called
// from all over the binary). The asset stores its source filename inline at asset+0x8. `param` is the
// caller's play request; its FIRST float (param+0x0) is the requested gain - the function early-outs
// silently when it is <= 0 - so scaling it attenuates just this one play, and returning 0 (the value
// the function's own early-outs return) suppresses the sound entirely. Both act only on this call, so
// mute / volume take effect instantly and reverse with no data destruction and no zoning.
const uintptr_t kAssetPlay = ::Rcp::eqva(0x5BFBD0);
constexpr int kOffAssetName = 0x8;

typedef int(__fastcall *AssetPlayFn)(void *asset, int edx, void *param);
AssetPlayFn g_orig = nullptr;

constexpr char kIniSection[] = "Sounds";
constexpr char kOverrideSection[] = "SoundOverrides";  // key = sound stem, value = volume percent (0..kMaxVolPct)
constexpr int kMaxVolPct = 300;  // 0 = mute, 100 = unchanged, up to 300 = boost quiet sounds
int clamp_vol(int pct) { return pct < 0 ? 0 : (pct > kMaxVolPct ? kMaxVolPct : pct); }

// A named block category (a preset group of filenames) - kept for the "Disable thunder" checkbox on
// the Sounds tab. Adding a preset type is one row here (plus a checkbox). Individual sounds are managed
// dynamically through the history/overrides below, not here.
struct SoundCategory {
  const char *key;
  const char *ini_key;
  bool enabled;
  std::vector<std::string> names;
};

SoundCategory g_categories[] = {
    {"thunder", "DisableThunder", false, {"thunder1.wav", "thunder2.wav"}},
};

// Per-sound record: how often/when it played, plus an optional volume override. vol_pct == -1 means no
// override (plays at 100%); 0 means muted; 1..100 scales the gain. Keyed by the lowercased basename
// stem so thunder1 / thunder1.wav / snd\thunder1.WAV are one entry.
struct SoundInfo {
  std::string display;   // name as first seen (for the printed list)
  unsigned count = 0;    // times played this session
  unsigned last_tick = 0;  // GetTickCount of the last play (for "recent" ordering)
  int vol_pct = -1;      // -1 = no override, 0 = mute, 1..100 = volume
};

std::mutex g_mu;                       // guards g_sounds (touched from the play hook + the commands)
std::map<std::string, SoundInfo> g_sounds;
constexpr size_t kMaxSounds = 512;     // bound the history/override map
std::vector<std::string> g_recent_order;  // stems from the last "/rcpsound recent", for "#" references
bool g_log_names = false;              // /rcpsound log: also log names to the file as they play
bool g_any_category = false;           // cached: is any preset category enabled?

// Lowercased basename stem of a filename: drop any path prefix and the extension.
std::string name_stem(const char *raw) {
  std::string s = raw;
  size_t slash = s.find_last_of("\\/");
  if (slash != std::string::npos) s.erase(0, slash + 1);
  size_t dot = s.find_last_of('.');
  if (dot != std::string::npos) s.erase(dot);
  for (char &ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}
std::string name_stem(const std::string &s) { return name_stem(s.c_str()); }

void recompute_any_category() {
  g_any_category = false;
  for (const SoundCategory &c : g_categories)
    if (c.enabled) g_any_category = true;
}

// True if `stem` is in any enabled preset category (e.g. thunder). Caller holds g_mu.
bool category_muted(const std::string &stem) {
  if (!g_any_category) return false;
  for (const SoundCategory &c : g_categories) {
    if (!c.enabled) continue;
    for (const std::string &n : c.names)
      if (stem == name_stem(n)) return true;
  }
  return false;
}

// Persist one per-sound override to [SoundOverrides] (stem = percent); vol_pct < 0 deletes it. Only
// touches the ini file (no g_sounds access), so it is called outside the g_mu lock.
void save_override(const std::string &stem, int vol_pct) {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (vol_pct < 0)
    ini.deleteKey(kOverrideSection, stem);
  else
    ini.setValue<int>(kOverrideSection, stem, vol_pct);
}

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  for (SoundCategory &c : g_categories)
    if (ini.exists(kIniSection, c.ini_key)) c.enabled = ini.getValue<bool>(kIniSection, c.ini_key);
  if (ini.exists(kIniSection, "LogNames")) g_log_names = ini.getValue<bool>(kIniSection, "LogNames");
  recompute_any_category();

  std::lock_guard<std::mutex> lk(g_mu);
  for (auto &kv : ini.getSection(kOverrideSection)) {
    const std::string stem = name_stem(kv.first);  // stored keys are already stems; normalize anyway
    if (stem.empty()) continue;
    int pct = std::atoi(kv.second.c_str());
    pct = clamp_vol(pct);
    SoundInfo &si = g_sounds[stem];
    si.vol_pct = pct;
    if (si.display.empty()) si.display = kv.first;
  }
}

void save_category(const SoundCategory &c) {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, c.ini_key, c.enabled);
}

SoundCategory *find_category(const std::string &key) {
  for (SoundCategory &c : g_categories)
    if (key == c.key) return &c;
  return nullptr;
}

// /rcpsound log: log each distinct sound name once (bounded), useful as a file record.
void maybe_log_name(const char *name) {
  static std::set<std::string> seen;  // NOLINT - local diagnostic bound
  if (seen.size() >= 512) return;
  if (!seen.insert(name).second) return;
  logger::logf("[sound] played: \"%s\"", name);
}

// The detour. Reads the asset's filename under the crash guard (a raw read that could fault on a stale
// asset), records it in the history, and applies any mute/volume - all string/map work is off the guard
// (heap only, can't fault), mirroring chat_shortcuts.cpp. Muted -> return 0 (skip). Volume < 100 ->
// transiently scale the request gain (param+0x0) around the original call, then restore it so the
// caller's struct is left untouched. Everything else tail-calls the original unchanged.
int __fastcall AssetPlay_hk(void *asset, int edx, void *param) {
  if (!asset || crash_handler::shutting_down()) return g_orig(asset, edx, param);

  char name[260];
  name[0] = '\0';
  const bool read_ok = rcp_guard::run("sound.play", [&] {
    const char *src = reinterpret_cast<const char *>(reinterpret_cast<char *>(asset) + kOffAssetName);
    int i = 0;
    for (; i < 259 && src[i]; ++i) name[i] = src[i];
    name[i] = '\0';
  });
  if (!read_ok || !name[0]) return g_orig(asset, edx, param);

  if (g_log_names) maybe_log_name(name);

  bool muted = false;
  int vol_pct = 100;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    const std::string stem = name_stem(name);
    auto it = g_sounds.find(stem);
    if (it == g_sounds.end() && g_sounds.size() < kMaxSounds) {
      it = g_sounds.emplace(stem, SoundInfo{}).first;
      it->second.display = name;
    }
    int ov = -1;
    if (it != g_sounds.end()) {
      it->second.count++;
      it->second.last_tick = GetTickCount();
      ov = it->second.vol_pct;
    }
    muted = (ov == 0) || category_muted(stem);
    if (!muted && ov > 0) vol_pct = ov;
  }

  if (muted) return 0;  // suppress this sound

  if (vol_pct != 100 && param) {  // scale the requested gain down (<100%) or up (>100%, to boost quiet sounds)
    float *gain = reinterpret_cast<float *>(param);
    float saved = 0.f;
    bool scaled = false;
    rcp_guard::run("sound.vol", [&] {
      saved = *gain;
      *gain = saved * (vol_pct / 100.0f);
      scaled = true;
    });
    if (!scaled) return g_orig(asset, edx, param);
    const int r = g_orig(asset, edx, param);
    rcp_guard::run("sound.vol", [&] { *gain = saved; });  // restore the caller's struct
    return r;
  }
  return g_orig(asset, edx, param);
}

// ---- command helpers ----

std::string vol_desc(int vol_pct) {
  if (vol_pct < 0) return "";
  if (vol_pct == 0) return "  [MUTED]";
  return "  [" + std::to_string(vol_pct) + "%]";
}

// Resolve a "<name|#>" argument to a sound stem. A pure number indexes the last "/rcpsound recent"
// list (1-based); anything else is treated as a filename and reduced to its stem (so it works for
// sounds not yet played too). Returns "" if a number is out of range.
std::string resolve_target(const std::string &arg) {
  bool numeric = !arg.empty() && std::all_of(arg.begin(), arg.end(), [](char c) { return std::isdigit((unsigned char)c); });
  if (numeric) {
    std::lock_guard<std::mutex> lk(g_mu);
    int idx = std::atoi(arg.c_str()) - 1;
    if (idx >= 0 && idx < static_cast<int>(g_recent_order.size())) return g_recent_order[idx];
    return "";
  }
  return name_stem(arg);
}

// Apply a volume override (vol_pct: -1 clear, 0 mute, 1..100). Creates the entry if needed so a not-
// yet-played sound can be pre-muted. Persists. Returns the display name.
std::string apply_override(const std::string &stem, int vol_pct) {
  std::string display;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    SoundInfo &si = g_sounds[stem];
    if (si.display.empty()) si.display = stem;
    si.vol_pct = vol_pct;
    display = si.display;
  }
  save_override(stem, vol_pct);
  return display;
}

void print_recent(int limit) {
  std::vector<std::pair<unsigned, std::string>> ordered;  // (last_tick, stem)
  {
    std::lock_guard<std::mutex> lk(g_mu);
    for (auto &kv : g_sounds)
      if (kv.second.count > 0) ordered.emplace_back(kv.second.last_tick, kv.first);
  }
  if (ordered.empty()) {
    Rcp::Game::print_chat("rof2ClientPlus: no sounds played yet.");
    return;
  }
  std::sort(ordered.begin(), ordered.end(), [](auto &a, auto &b) { return a.first > b.first; });  // newest first
  if (static_cast<int>(ordered.size()) > limit) ordered.resize(limit);

  std::lock_guard<std::mutex> lk(g_mu);
  g_recent_order.clear();
  Rcp::Game::print_chat("rof2ClientPlus recent sounds (newest first) - /rcpsound mute|vol #:");
  for (size_t i = 0; i < ordered.size(); ++i) {
    const std::string &stem = ordered[i].second;
    g_recent_order.push_back(stem);
    const SoundInfo &si = g_sounds[stem];
    Rcp::Game::print_chat("  %2d. %s  x%u%s", static_cast<int>(i + 1), si.display.c_str(), si.count,
                          vol_desc(si.vol_pct).c_str());
  }
}

void print_status() {
  std::string s;
  for (const SoundCategory &c : g_categories) {
    if (!s.empty()) s += ", ";
    s += c.key;
    s += c.enabled ? ": disabled" : ": on";
  }
  size_t overrides = 0;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    for (auto &kv : g_sounds)
      if (kv.second.vol_pct >= 0) ++overrides;
  }
  Rcp::Game::print_chat("rof2ClientPlus sounds - %s; %zu per-sound override(s); log %s. "
                        "'/rcpsound recent' to list, 'mute|unmute|vol <name|#>' to manage.",
                        s.c_str(), overrides, g_log_names ? "ON" : "off");
}

bool parse_toggle(const std::string *arg, bool current, bool *out) {
  if (!arg) {
    *out = !current;
    return true;
  }
  if (*arg == "on" || *arg == "1") { *out = true; return true; }
  if (*arg == "off" || *arg == "0") { *out = false; return true; }
  return false;
}

}  // namespace

namespace sound_settings {
bool get_thunder_disabled() {
  const SoundCategory *c = find_category("thunder");
  return c && c->enabled;
}
void set_thunder_disabled(bool on) {
  SoundCategory *c = find_category("thunder");
  if (!c || c->enabled == on) return;
  c->enabled = on;
  recompute_any_category();
  save_category(*c);
}

std::vector<TrackedSound> get_tracked() {
  std::vector<TrackedSound> out;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    for (auto &kv : g_sounds)
      if (kv.second.vol_pct >= 0)
        out.push_back({kv.first, kv.second.display.empty() ? kv.first : kv.second.display, kv.second.vol_pct});
  }
  std::sort(out.begin(), out.end(), [](const TrackedSound &a, const TrackedSound &b) { return a.display < b.display; });
  return out;
}

std::vector<std::string> get_recent_untracked(int max) {
  std::vector<std::pair<unsigned, std::string>> ordered;  // (last_tick, display)
  {
    std::lock_guard<std::mutex> lk(g_mu);
    for (auto &kv : g_sounds)
      if (kv.second.vol_pct < 0 && kv.second.count > 0)
        ordered.emplace_back(kv.second.last_tick, kv.second.display.empty() ? kv.first : kv.second.display);
  }
  std::sort(ordered.begin(), ordered.end(), [](auto &a, auto &b) { return a.first > b.first; });  // newest first
  std::vector<std::string> out;
  for (auto &p : ordered) {
    if (static_cast<int>(out.size()) >= max) break;
    out.push_back(p.second);
  }
  return out;
}

std::string add_tracked(const std::string &display_name) {
  std::string stem = name_stem(display_name);
  if (stem.empty()) return stem;
  bool newly = false;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    SoundInfo &si = g_sounds[stem];
    if (si.display.empty()) si.display = display_name;
    if (si.vol_pct < 0) {
      si.vol_pct = 100;
      newly = true;
    }
  }
  if (newly) save_override(stem, 100);
  return stem;
}

void set_volume(const std::string &stem, int pct) {
  if (stem.empty()) return;
  pct = clamp_vol(pct);
  {
    std::lock_guard<std::mutex> lk(g_mu);
    SoundInfo &si = g_sounds[stem];
    if (si.display.empty()) si.display = stem;
    si.vol_pct = pct;
  }
  save_override(stem, pct);
}

void remove_tracked(const std::string &stem) {
  if (stem.empty()) return;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_sounds.find(stem);
    if (it != g_sounds.end()) it->second.vol_pct = -1;
  }
  save_override(stem, -1);
}
}  // namespace sound_settings

SoundMods::SoundMods(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  rcp->hooks->Add("rcp_sound_play", static_cast<int>(kAssetPlay), AssetPlay_hk, hook_type_detour);
  g_orig = rcp->hooks->hook_map["rcp_sound_play"]->original(AssetPlay_hk);
  logger::logf("[sound] Asset::Play detour installed at 0x%x; thunder_disabled=%d log_names=%d",
               (unsigned)kAssetPlay, (int)sound_settings::get_thunder_disabled(), (int)g_log_names);

  rcp->commands_hook->Add(
      "/rcpsound", {},
      "Manage game sounds: '/rcpsound recent [N]' lists recent sounds; 'vol <name|#> <0-300>' sets a "
      "sound's volume (0 = mute); 'reset <name|#>' removes it; 'thunder on|off'; 'log on|off'; 'clear'.",
      [](std::vector<std::string> &args) {
        if (args.size() < 2) {
          print_status();
          return true;
        }
        const std::string sub = args[1];
        const std::string *a2 = args.size() >= 3 ? &args[2] : nullptr;

        if (sub == "recent" || sub == "list") {
          int n = a2 ? std::atoi(a2->c_str()) : 20;
          if (n <= 0) n = 20;
          print_recent(n);
          return true;
        }
        if (sub == "clear") {
          std::lock_guard<std::mutex> lk(g_mu);
          for (auto it = g_sounds.begin(); it != g_sounds.end();) {
            if (it->second.vol_pct >= 0) {  // keep overridden sounds; just reset their play stats
              it->second.count = 0;
              it->second.last_tick = 0;
              ++it;
            } else {
              it = g_sounds.erase(it);
            }
          }
          g_recent_order.clear();
          Rcp::Game::print_chat("rof2ClientPlus: sound history cleared (overrides kept).");
          return true;
        }
        if (sub == "log") {
          bool on;
          if (!parse_toggle(a2, g_log_names, &on)) {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpsound log on|off'");
            return true;
          }
          g_log_names = on;
          IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, "LogNames", g_log_names);
          Rcp::Game::print_chat("rof2ClientPlus sound-name logging: %s", g_log_names ? "ON" : "off");
          return true;
        }
        if (sub == "reset" || sub == "untrack" || sub == "remove") {
          if (!a2) {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpsound reset <name|#>'");
            return true;
          }
          std::string stem = resolve_target(*a2);
          if (stem.empty()) {
            Rcp::Game::print_chat("rof2ClientPlus: no sound '%s' (try '/rcpsound recent')", a2->c_str());
            return true;
          }
          std::string disp = apply_override(stem, -1);  // -1 = remove override -> plays normally, untracked
          Rcp::Game::print_chat("rof2ClientPlus: '%s' reset (removed from list)", disp.c_str());
          return true;
        }
        if (sub == "vol" || sub == "volume") {
          const std::string *a3 = args.size() >= 4 ? &args[3] : nullptr;
          if (!a2 || !a3) {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpsound vol <name|#> <0-300>'");
            return true;
          }
          std::string stem = resolve_target(*a2);
          if (stem.empty()) {
            Rcp::Game::print_chat("rof2ClientPlus: no sound '%s' (try '/rcpsound recent')", a2->c_str());
            return true;
          }
          int pct = std::atoi(a3->c_str());
          pct = clamp_vol(pct);
          std::string disp = apply_override(stem, pct);
          Rcp::Game::print_chat("rof2ClientPlus: '%s' volume -> %d%%%s", disp.c_str(), pct,
                                pct == 0 ? " (muted)" : "");
          return true;
        }

        SoundCategory *c = find_category(sub);
        if (!c) {
          Rcp::Game::print_chat("rof2ClientPlus: unknown '%s' (recent|vol|reset|thunder|log|clear)", sub.c_str());
          return true;
        }
        bool on;
        if (!parse_toggle(a2, c->enabled, &on)) {
          Rcp::Game::print_chat("rof2ClientPlus: '/rcpsound %s on|off'", c->key);
          return true;
        }
        c->enabled = on;
        recompute_any_category();
        save_category(*c);
        Rcp::Game::print_chat("rof2ClientPlus %s sounds: %s", c->key, c->enabled ? "DISABLED" : "on");
        return true;
      });
  logger::log("[sound] /rcpsound registered");
}
