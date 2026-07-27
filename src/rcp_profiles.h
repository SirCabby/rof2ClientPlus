// rof2ClientPlus - named settings profiles.
//
// A profile is a complete set of the mod's settings under one name, so a main can
// run everything (nameplates, models, long view distance) while alts run a trimmed,
// cheaper set. Profiles are switched from the /rcpoptions General tab or /rcpprofile,
// and EVERY character remembers the profile it last used: on world entry the mod
// switches to that character's profile automatically (no opt-in - a character seen
// for the first time simply adopts whatever profile is active and remembers it).
//
// STORAGE (rof2ClientPlus.ini). The bookkeeping lives in a [Profiles] section that is
// itself never profiled:
//
//   [Profiles]
//   Active=Alt
//   List=Default,Alt
//   Char_Sircabby=Default
//
// The DEFAULT profile's settings are the ini's PLAIN sections ([Nameplate], [Mouse],
// ...), so an ini written before profiles existed already IS the default profile -
// there is no migration. Every other profile stores the same sections behind a
// "<Profile>." prefix ([Alt.Nameplate], [Alt.Mouse], ...). All of that mapping happens
// in ONE place - IO_ini::sec() calls map_ini_section() - so feature code keeps writing
// its own plain section name and never knows profiles exist.
//
// WHICH SETTINGS ARE PROFILED is two allowlists in rcp_profiles.cpp, and they are the
// single knob for the feature's scope: kProfiledSections (whole sections - the model
// swaps, whose keys are all one feature) and kProfiledKeys (individual keys, e.g. only
// [Font] MaxDist while the rest of [Font] stays shared). Anything unlisted is global.
//
// SWITCHING re-points the mapping, then calls every module's registered reload
// handler, which re-reads that module's settings and re-applies them live. Any key
// the incoming profile does not define is first copied ("pinned") from the profile
// being left, so a switch can never leave a stale value silently inherited from
// whatever was loaded before.
#pragma once

#include <functional>
#include <string>
#include <vector>

class RcpService;

namespace rcp_profiles {

// The built-in profile stored in the ini's plain (unprefixed) sections.
extern const char *const kDefaultProfile;

constexpr int kMaxProfiles = 8;    // Keeps [Profiles] List inside IO_ini's 256-byte read.
constexpr int kMaxNameLength = 24;

// Reads [Profiles] and makes the persisted active profile current. MUST run before any
// feature module loads its settings (RcpService calls it right after the ini is up).
void init();

const std::vector<std::string> &list();  // Always starts with kDefaultProfile.
std::string active();
bool exists(const std::string &name);
bool valid_name(const std::string &name);  // 1..kMaxNameLength of [A-Za-z0-9 _-].

// Makes `name` the active profile: pins anything it does not define from the profile
// being left, persists the choice (globally and for the logged-in character), and
// broadcasts to every reload handler. False if the profile does not exist.
bool switch_to(const std::string &name);

bool create(const std::string &name);  // New profile cloned from the active one (does not switch).
bool remove(const std::string &name);  // Deletes its sections; switches to Default if it was active.
bool rename_profile(const std::string &from, const std::string &to);

// Per-character memory ([Profiles] Char_<name>); "" when unknown or stale.
std::string char_profile(const std::string &character);
std::string current_character();  // "" unless in game with a named character.

// Applies the logged-in character's remembered profile. Driven every frame from
// dllmain's ProcessGameEvents hook; does real work only when the character changes.
void on_frame();

// Modules register a handler that re-reads their ini settings and re-applies them
// live. Handlers run (guarded, in registration order) on every profile switch.
void add_reload_handler(std::function<void()> fn);

// One handler that runs AFTER every reload handler, whatever order the modules were
// constructed in. The options window uses it to repaint itself from the finished state
// (registering it as a normal reload handler would repaint from a half-loaded one).
void set_reload_finished_handler(std::function<void()> fn);

// Called by IO_ini for the mod ini: the physical section for a logical one. The
// section-only form is for whole-section operations (getSection/deleteSection) and maps
// only fully-profiled sections; the section+key form also maps individually profiled keys.
std::string map_ini_section(const std::string &section);
std::string map_ini_section(const std::string &section, const std::string &key);

}  // namespace rcp_profiles

// Owns the /rcpprofile command. Constructed last, after every module has registered
// its reload handler.
class RcpProfiles {
 public:
  explicit RcpProfiles(RcpService *rcp);
};
