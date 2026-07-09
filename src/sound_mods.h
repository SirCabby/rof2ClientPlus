// rof2ClientPlus - disable specific game sounds by name (the /rcpsound feature).
//
// RoF2 loads every sound from its snd*.pfs archives by filename into an audio-asset
// object, then plays it by index. The exe never names individual wavs (thunder1.wav /
// thunder2.wav etc. come from the packaged data), but each asset keeps its own
// filename inline at asset+0x8, and EVERY sound start funnels through one method,
// Asset::Play @ 0x5BFBD0 (__thiscall(this=asset, param), ret 4). So we detour that
// method: read the live filename off `this`, and if it matches an enabled block
// category, return 0 (the "did not play" result the function already produces on its
// own early-outs, which callers handle) - suppressing the sound. Otherwise we tail-
// call the original.
//
// The check is per-play against the current settings, so toggling a category mutes /
// unmutes instantly, both directions, with no data destruction and no zoning. The
// block list is organized into named categories (thunder to start) so more sound
// types can be added with a single row + checkbox later.
//
// '/rcpsound thunder on|off' toggles the thunder category; '/rcpsound log on|off'
// prints the distinct sound names as they play (to discover names for new
// categories); the choices persist in rof2ClientPlus.ini [Sounds].
#pragma once

#include <string>
#include <vector>

class RcpService;

class SoundMods {
 public:
  explicit SoundMods(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for the options-window UI (the Sounds tab).
namespace sound_settings {
bool get_thunder_disabled();         // true = thunder1/thunder2 wavs are muted (preset category).
void set_thunder_disabled(bool on);  // Applies live (next play) + persists to ini.

// A sound the user is managing (has an override entry). vol_pct: 0 = muted, 1..100 = volume.
struct TrackedSound {
  std::string stem;     // stable identity (lowercased basename)
  std::string display;  // name to show
  int vol_pct;          // 0..100
};

// The tracked list (sorted by display name) - the sounds the user has added/changed.
std::vector<TrackedSound> get_tracked();

// Recently played sounds NOT yet tracked, newest first, up to `max` - the "add to list" candidates.
std::vector<std::string> get_recent_untracked(int max);

// Start tracking a played sound (by display name); defaults to 100% volume if not already tracked.
// Returns the sound's stem (its stable identity) so the caller can select it, or "" if the name is empty.
std::string add_tracked(const std::string &display_name);

// Set a tracked sound's volume (0..100; 0 = mute). Persists. Applies on the sound's next play.
void set_volume(const std::string &stem, int pct);

// Stop tracking a sound: remove its override so it plays normally again, and drop it from the list.
void remove_tracked(const std::string &stem);
}  // namespace sound_settings
