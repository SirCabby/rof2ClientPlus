// rof2ClientPlus - Zeal-style chat token shortcuts.
//
// Expands short tokens typed into any outgoing chat line (say/tell/group/guild/...)
// into live values, the same way Zeal does:
//   %n    -> current mana percent   (e.g. "73%")
//   %h    -> current hit-point percent
//   %loc  -> your location          ("Y, X, Z", matching the client's /loc order)
//   %thp  -> your target's HP percent
//
// Always on (no toggle) - implemented as a detour on CEverQuest::DoPercentConvert
// (the client function that already expands the stock %t/%s/... tokens on outgoing
// chat). We substitute our tokens first, then let the client convert the rest.
#pragma once

class RcpService;

class ChatShortcuts {
 public:
  ChatShortcuts(RcpService *rcp);
  ~ChatShortcuts();
};
