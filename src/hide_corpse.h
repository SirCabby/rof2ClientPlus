// rof2ClientPlus - continuous corpse hiding (/hidecorpse always + showlast).
//
// RoF2's native /hidecorpses (plural) hides only EXISTING corpses once
// (server-mediated), explicitly does NOT hide corpses that spawn later, and has no
// way to reveal a single corpse. This module EXTENDS that stock command with the two
// Zeal-style conveniences it lacks (the command hook intercepts /hidecorpses, handles
// these keywords, and forwards the native ones):
//   /hidecorpses always   - toggle: keep every NPC corpse hidden continuously,
//                           including ones that spawn after the command (a per-frame
//                           re-assert over the spawn list). Player corpses are never
//                           touched, so your own / others' lootable corpses stay
//                           visible.
//   /hidecorpses showlast - reveal the single most-recently-hidden NPC corpse (e.g.
//                           the mob you just killed) so you can see/loot it; it stays
//                           visible until it despawns or you toggle always off.
// The native NONE/ALL/ALLBUTGROUP/LOOTED/NPC keywords keep working unchanged.
// State persists to rof2ClientPlus.ini [HideCorpse].
#pragma once

class RcpService;

class HideCorpse {
 public:
  explicit HideCorpse(RcpService *rcp);
  ~HideCorpse();

 private:
  RcpService *rcp_;
};

// Shared accessors (also used by a future options-UI checkbox, mirroring the other
// feature modules' *_settings namespaces).
namespace hide_corpse_settings {
bool get_always();
void set_always(bool on);
bool get_looted();
void set_looted(bool on);
}  // namespace hide_corpse_settings
