// rof2ClientPlus - named settings profiles. See rcp_profiles.h for the design.
#include "rcp_profiles.h"

#include <cstdio>
#include <cstring>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"
#include "game_structures.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// ---------------------------------------------------------------------------
// WHAT A PROFILE COVERS - the two ALLOWLISTS below, and nothing else.
//
// Anything not listed is GLOBAL: one copy shared by every profile (which is also
// how the [Profiles] bookkeeping stays outside the system). Moving a setting in or
// out of profiles is a one-line edit here and nothing else - no feature code knows
// profiles exist.
//
// Whole sections, for the open-ended key sets where every key belongs to the same
// feature (the model swaps: one key per item / creature / PC race).
// ---------------------------------------------------------------------------
constexpr const char *kProfiledSections[] = {
    "Models",     // item graphics, classic vs modern (model_swap)
    "NpcModels",  // creature models (npc_model_swap)
    "PcModels",   // PC race/gender models (npc_model_swap)
};

// Individual keys, for features where only ONE setting is worth splitting. The rest
// of their section stays global - e.g. [Font] MaxDist follows the profile while every
// other billboard-nameplate setting is shared.
struct ProfiledKey {
  const char *section;
  const char *key;
};
constexpr ProfiledKey kProfiledKeys[] = {
    {"Font", "MaxDist"},            // nameplate draw distance (Nameplate tab slider / '/rcpfont dist')
    {"Fog", "RemoveDistanceFog"},   // remove distance fog ('/rcpfog')
    {"ViewDistance", "FarClip"},    // terrain view distance
    {"ViewDistance", "ActorClip"},  // actor (NPC/player) draw distance
    {"FloatingDamage", "Enabled"},  // floating combat damage on/off ('/rcpfcd')
};

constexpr char kSection[] = "Profiles";  // Bookkeeping; never profiled (absent from the allowlist).
constexpr char kKeyActive[] = "Active";
constexpr char kKeyList[] = "List";
constexpr char kCharKeyPrefix[] = "Char_";

std::string g_active;   // Current profile name (kDefaultProfile until init()).
std::string g_prefix;   // "" for the default profile, else "<Profile>.".
std::vector<std::string> g_list;
char g_seen_char[64] = {0};  // Character on_frame() last applied a profile for.
bool g_ready = false;        // init() has run (guards on_frame + every mutation).

// Function-local storage: modules register from their constructors, which run before
// any dynamic initializer in this file would be guaranteed to have run.
std::vector<std::function<void()>> &handlers() {
  static std::vector<std::function<void()>> h;
  return h;
}

// Runs after all of the above (the options window repaints itself).
std::function<void()> &finished_handler() {
  static std::function<void()> h;
  return h;
}

// Raw (unmapped) view of the mod ini - profile bookkeeping and whole-profile copies
// must address physical sections, not the active profile's view of them.
IO_ini raw() { return IO_ini(IO_ini::kRcpIniFilename, false); }

bool iequals(const std::string &a, const std::string &b) {
  return a.size() == b.size() && _strnicmp(a.c_str(), b.c_str(), a.size()) == 0;
}

std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t");
  return s.substr(b, e - b + 1);
}

bool is_default(const std::string &name) { return iequals(name, rcp_profiles::kDefaultProfile); }

// The physical section a profile stores `section` in.
std::string physical(const std::string &profile, const std::string &section) {
  return is_default(profile) ? section : profile + "." + section;
}

// Copies everything a profile owns from one profile to another. only_missing=true
// "pins" values the destination does not define yet (used on a switch) instead of
// overwriting it (used when cloning into a brand-new profile).
void copy_profile(const std::string &from, const std::string &to, bool only_missing) {
  IO_ini ini = raw();  // Raw: these are physical section names already.
  int copied = 0;
  for (const char *s : kProfiledSections) {
    const std::string src = physical(from, s), dst = physical(to, s);
    for (const auto &kv : ini.getSection(src)) {
      if (only_missing && ini.exists(dst, kv.first)) continue;
      ini.setValue<std::string>(dst, kv.first, kv.second);
      ++copied;
    }
  }
  for (const ProfiledKey &pk : kProfiledKeys) {
    const std::string src = physical(from, pk.section), dst = physical(to, pk.section);
    if (!ini.exists(src, pk.key)) continue;  // Never set anywhere yet: leave it at the code default.
    if (only_missing && ini.exists(dst, pk.key)) continue;
    ini.setValue<std::string>(dst, pk.key, ini.getValue<std::string>(src, pk.key));
    ++copied;
  }
  logger::logf("[profiles] copied %d key(s) '%s' -> '%s'%s", copied, from.c_str(), to.c_str(),
               only_missing ? " (missing only)" : "");
}

// Drops a profile's stored settings. A non-default profile's prefixed sections only ever
// receive the keys it owns (that is all the mapping ever routes there), so deleting the
// whole prefixed section is exact even for the per-key entries.
void erase_profile_sections(const std::string &name) {
  if (is_default(name)) return;  // The default profile IS the plain ini; never wipe it.
  IO_ini ini = raw();
  for (const char *s : kProfiledSections) ini.deleteSection(physical(name, s));
  for (const ProfiledKey &pk : kProfiledKeys) ini.deleteSection(physical(name, pk.section));
}

std::vector<std::string> split_csv(const std::string &s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t comma = s.find(',', start);
    if (comma == std::string::npos) {
      out.push_back(trim(s.substr(start)));
      break;
    }
    out.push_back(trim(s.substr(start, comma - start)));
    start = comma + 1;
  }
  return out;
}

void write_list() {
  std::string csv;
  for (const std::string &n : g_list) {
    if (!csv.empty()) csv += ",";
    csv += n;
  }
  raw().setValue<std::string>(kSection, kKeyList, csv);
}

// Points the section mapping at `name` and persists it. broadcast=true re-runs every
// module's reload handler (each guarded, so one bad handler cannot take the client down).
void apply_active(const std::string &name, bool broadcast) {
  g_active = name;
  g_prefix = is_default(name) ? "" : name + ".";
  raw().setValue<std::string>(kSection, kKeyActive, name);
  if (!broadcast) return;
  int n = 0;
  for (auto &h : handlers()) {
    rcp_guard::run("profiles.reload", [&] { h(); });
    ++n;
  }
  if (finished_handler()) rcp_guard::run("profiles.reload.ui", [&] { finished_handler()(); });
  logger::logf("[profiles] active -> '%s' (%d reload handlers)", name.c_str(), n);
}

std::string char_key(const std::string &character) { return std::string(kCharKeyPrefix) + character; }

void remember_char(const std::string &character, const std::string &profile) {
  if (character.empty()) return;
  raw().setValue<std::string>(kSection, char_key(character), profile);
}

}  // namespace

namespace rcp_profiles {

const char *const kDefaultProfile = "Default";

const std::vector<std::string> &list() { return g_list; }

std::string active() { return g_active.empty() ? kDefaultProfile : g_active; }

bool exists(const std::string &name) {
  for (const std::string &n : g_list)
    if (iequals(n, name)) return true;
  return false;
}

bool valid_name(const std::string &name) {
  if (name.empty() || name.size() > kMaxNameLength) return false;
  for (char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
                    c == '_' || c == '-';
    if (!ok) return false;  // '.' , ',' and '[' would break the section prefix / list encoding.
  }
  return true;
}

// The one place the profile namespace is applied. Hot-ish (every ini read/write), so
// the default profile costs a single empty-string test.
//
// Section only (IO_ini::getSection / deleteSection - a whole-section operation): maps
// only for sections the profile owns outright. A section that merely contains a profiled
// KEY is NOT redirected wholesale, or a bulk read would miss its global keys.
std::string map_ini_section(const std::string &section) {
  if (g_prefix.empty()) return section;
  for (const char *s : kProfiledSections)
    if (iequals(section, s)) return g_prefix + section;
  return section;
}

// Section + key (every single-value read/write): also maps the individually profiled keys.
std::string map_ini_section(const std::string &section, const std::string &key) {
  if (g_prefix.empty()) return section;
  for (const char *s : kProfiledSections)
    if (iequals(section, s)) return g_prefix + section;
  for (const ProfiledKey &pk : kProfiledKeys)
    if (iequals(section, pk.section) && iequals(key, pk.key)) return g_prefix + section;
  return section;
}

void add_reload_handler(std::function<void()> fn) {
  if (fn) handlers().push_back(std::move(fn));
}

void set_reload_finished_handler(std::function<void()> fn) { finished_handler() = std::move(fn); }

void init() {
  if (g_ready) return;  // Runs at DllMain attach; RcpService's later call is a no-op.
  IO_ini ini = raw();
  g_list.clear();
  g_list.push_back(kDefaultProfile);
  for (const std::string &n : split_csv(ini.getValue<std::string>(kSection, kKeyList))) {
    if (!valid_name(n) || exists(n)) continue;
    if (static_cast<int>(g_list.size()) >= kMaxProfiles) break;
    g_list.push_back(n);
  }
  g_active = kDefaultProfile;
  g_prefix.clear();
  const std::string saved = trim(ini.getValue<std::string>(kSection, kKeyActive));
  if (!saved.empty() && exists(saved)) {
    for (const std::string &n : g_list)  // Adopt the stored spelling from the list.
      if (iequals(n, saved)) {
        g_active = n;
        break;
      }
    g_prefix = is_default(g_active) ? "" : g_active + ".";
  }
  write_list();
  ini.setValue<std::string>(kSection, kKeyActive, g_active);
  g_ready = true;
  logger::logf("[profiles] init: active='%s', %d profile(s), %d profiled section(s)", g_active.c_str(),
               (int)g_list.size(), (int)(sizeof(kProfiledSections) / sizeof(kProfiledSections[0])));
}

// pin=false skips carrying the outgoing profile's values over - used when the profile we
// are leaving is being deleted, so a discarded profile cannot seed the one we land on.
static bool switch_internal(const std::string &name, bool pin) {
  if (!g_ready || !exists(name)) return false;
  const std::string character = current_character();
  if (iequals(name, g_active)) {
    remember_char(character, g_active);  // Re-affirm this character's choice.
    return true;
  }
  // Pin anything the incoming profile has no opinion on, so it cannot silently
  // inherit the outgoing profile's live value (settings only load keys that exist).
  if (pin) copy_profile(g_active, name, /*only_missing=*/true);
  apply_active(name, /*broadcast=*/true);
  remember_char(character, g_active);
  return true;
}

bool switch_to(const std::string &name) { return switch_internal(name, /*pin=*/true); }

bool create(const std::string &name) {
  if (!g_ready || !valid_name(name) || exists(name)) return false;
  if (static_cast<int>(g_list.size()) >= kMaxProfiles) return false;
  g_list.push_back(name);
  write_list();
  copy_profile(g_active, name, /*only_missing=*/false);  // Start as a copy of what you are running now.
  logger::logf("[profiles] created '%s' from '%s'", name.c_str(), g_active.c_str());
  return true;
}

bool remove(const std::string &name) {
  if (!g_ready || is_default(name) || !exists(name)) return false;
  if (iequals(name, g_active)) switch_internal(kDefaultProfile, /*pin=*/false);
  erase_profile_sections(name);
  for (size_t i = 0; i < g_list.size(); ++i)
    if (iequals(g_list[i], name)) {
      g_list.erase(g_list.begin() + i);
      break;
    }
  write_list();
  logger::logf("[profiles] deleted '%s'", name.c_str());
  return true;  // Stale Char_* entries are ignored by char_profile() and rewritten on next login.
}

bool rename_profile(const std::string &from, const std::string &to) {
  if (!g_ready || !exists(from) || is_default(from)) return false;
  if (!valid_name(to) || exists(to)) return false;
  copy_profile(from, to, /*only_missing=*/false);
  erase_profile_sections(from);
  for (std::string &n : g_list)
    if (iequals(n, from)) n = to;
  write_list();
  // Re-point every character that remembered the old name (and us, if it was active).
  IO_ini ini = raw();
  for (const auto &kv : ini.getSection(kSection)) {
    if (kv.first.compare(0, std::strlen(kCharKeyPrefix), kCharKeyPrefix) != 0) continue;
    if (iequals(trim(kv.second), from)) ini.setValue<std::string>(kSection, kv.first, to);
  }
  if (iequals(g_active, from)) apply_active(to, /*broadcast=*/false);  // Same values, no reload needed.
  logger::logf("[profiles] renamed '%s' -> '%s'", from.c_str(), to.c_str());
  return true;
}

std::string char_profile(const std::string &character) {
  if (character.empty()) return "";
  const std::string want = trim(raw().getValue<std::string>(kSection, char_key(character)));
  return exists(want) ? want : "";
}

std::string current_character() {
  if (!Rcp::Game::is_in_game()) return "";
  char buf[64] = {0};
  rcp_guard::run("profiles.charname", [&] {
    Rcp::GameStructures::GAMECHARINFO *info = Rcp::Game::get_char_info();
    if (info) std::snprintf(buf, sizeof(buf), "%s", info->Name);
  });
  return buf;
}

// World entry / character switch: load that character's profile. A character we have
// never seen adopts (and remembers) whatever profile is active - no opt-in needed.
void on_frame() {
  if (!g_ready) return;
  if (!Rcp::Game::is_in_game()) {
    g_seen_char[0] = 0;  // Re-check on the next world entry.
    return;
  }
  const std::string character = current_character();
  if (character.empty() || character == g_seen_char) return;
  std::snprintf(g_seen_char, sizeof(g_seen_char), "%s", character.c_str());

  const std::string want = char_profile(character);
  if (want.empty()) {
    remember_char(character, g_active);
    logger::logf("[profiles] %s: first login, adopting '%s'", character.c_str(), g_active.c_str());
    return;
  }
  if (iequals(want, g_active)) return;
  if (switch_to(want))
    Rcp::Game::print_chat("rof2ClientPlus: settings profile '%s' loaded for %s.", want.c_str(), character.c_str());
}

}  // namespace rcp_profiles

// ---------------------------------------------------------------------------
// /rcpprofile
// ---------------------------------------------------------------------------
namespace {

void print_status() {
  std::string names;
  for (const std::string &n : rcp_profiles::list()) {
    if (!names.empty()) names += ", ";
    names += n;
    if (iequals(n, rcp_profiles::active())) names += " (active)";
  }
  Rcp::Game::print_chat("rof2ClientPlus profiles: %s", names.c_str());
  const std::string character = rcp_profiles::current_character();
  if (character.empty()) return;
  const std::string remembered = rcp_profiles::char_profile(character);
  Rcp::Game::print_chat("  %s loads '%s' at login. Switching profiles re-points that memory.", character.c_str(),
                        remembered.empty() ? rcp_profiles::active().c_str() : remembered.c_str());
}

// Joins the rest of a command's arguments so profile names may contain spaces.
std::string join_args(const std::vector<std::string> &args, size_t first) {
  std::string out;
  for (size_t i = first; i < args.size(); ++i) {
    if (!out.empty()) out += " ";
    out += args[i];
  }
  return out;
}

}  // namespace

RcpProfiles::RcpProfiles(RcpService *rcp) {
  rcp->commands_hook->Add(
      "/rcpprofile", {"/rcpprofiles"},
      "Settings profiles: '/rcpprofile' status, '/rcpprofile <name>' switch, '/rcpprofile new <name>' "
      "(copy of the current one), '/rcpprofile delete <name>', '/rcpprofile rename <name>'. Each character "
      "remembers the profile it last used.",
      [](std::vector<std::string> &args) {
        if (args.size() < 2) {
          print_status();
          return true;
        }
        const std::string verb = args[1];
        if (iequals(verb, "list")) {
          print_status();
          return true;
        }
        if (iequals(verb, "new") || iequals(verb, "create") || iequals(verb, "copy")) {
          const std::string name = join_args(args, 2);
          if (!rcp_profiles::valid_name(name)) {
            Rcp::Game::print_chat("rof2ClientPlus: bad profile name (letters, digits, space, _ and -; max %d).",
                                  rcp_profiles::kMaxNameLength);
            return true;
          }
          if (!rcp_profiles::create(name)) {
            Rcp::Game::print_chat("rof2ClientPlus: could not create '%s' (already exists, or %d-profile limit).",
                                  name.c_str(), rcp_profiles::kMaxProfiles);
            return true;
          }
          rcp_profiles::switch_to(name);
          Rcp::Game::print_chat("rof2ClientPlus: profile '%s' created and active.", name.c_str());
          return true;
        }
        if (iequals(verb, "delete") || iequals(verb, "remove")) {
          const std::string name = join_args(args, 2);
          if (!rcp_profiles::remove(name)) {
            Rcp::Game::print_chat("rof2ClientPlus: cannot delete '%s' (unknown, or the Default profile).",
                                  name.c_str());
            return true;
          }
          Rcp::Game::print_chat("rof2ClientPlus: profile '%s' deleted; now on '%s'.", name.c_str(),
                                rcp_profiles::active().c_str());
          return true;
        }
        if (iequals(verb, "rename")) {
          const std::string name = join_args(args, 2);
          const std::string from = rcp_profiles::active();
          if (!rcp_profiles::rename_profile(from, name)) {
            Rcp::Game::print_chat("rof2ClientPlus: cannot rename '%s' to '%s'.", from.c_str(), name.c_str());
            return true;
          }
          Rcp::Game::print_chat("rof2ClientPlus: profile '%s' renamed to '%s'.", from.c_str(), name.c_str());
          return true;
        }
        // Anything else is a profile name to switch to.
        const std::string name = join_args(args, 1);
        if (!rcp_profiles::switch_to(name)) {
          Rcp::Game::print_chat("rof2ClientPlus: no profile '%s' ('/rcpprofile new %s' creates it).", name.c_str(),
                                name.c_str());
          return true;
        }
        Rcp::Game::print_chat("rof2ClientPlus: settings profile '%s' active.", rcp_profiles::active().c_str());
        return true;
      });
  logger::log("[profiles] /rcpprofile registered");
}
