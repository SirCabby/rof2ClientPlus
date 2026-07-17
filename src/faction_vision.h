// rof2ClientPlus - "faction vision": surface faction id + value on kills and consider.
//
// The cabby-enhanced EQEmu server (rule Client:RcpFactionVision) emits a machine-tagged
// line through the normal chat path whenever your faction changes on a kill/quest hit
// (Client::SendFactionMessage) or when you consider an NPC (Handle_OP_Consider):
//
//   [RcpFac]t=kill|id=42|d=-15|cur=1234|min=-2000|max=2000|nm=Claws of Veeshan
//   [RcpFac]t=con|id=42|cur=1234|eff=1180|nm=Claws of Veeshan
//
// We detour the client's central chat-display choke (CEverQuest::dsp_chat @ 0x51F1A0 -
// the same one ChatTimestamp uses) and, for lines that carry the [RcpFac] tag, parse the
// fields and rewrite the line into a readable form before the client renders it (the raw
// tag is never shown). This module is installed AFTER ChatTimestamp, so it runs as the
// outermost hook - it sees the raw server text (before any timestamp) and its rewritten
// line then flows through the timestamp hook, so timestamps still apply. The hook_wrapper
// detour explicitly supports stacking on an already-hooked function (see hook_wrapper.cpp).
//
// '/rcpfaction on|off' toggles the client-side rewrite (persisted in rof2ClientPlus.ini
// [Faction]); default on. Turning it off shows the raw tag; to stop the lines entirely,
// disable the server rule.
#pragma once

#include <string>

class RcpService;

class FactionVision {
 public:
  explicit FactionVision(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for a future options-window tab and the /rcpfaction command.
namespace faction_vision_settings {
bool get_enabled();         // true = [RcpFac] lines are rewritten into readable text.
void set_enabled(bool on);  // Applies live (next line) + persists to ini.
}  // namespace faction_vision_settings
