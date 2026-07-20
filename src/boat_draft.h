// rof2ClientPlus - boat draft offset (/rcpboat).
//
// RoF2 computes a ship's waterline CLIENT-side: the server's z for race-72/73
// boat spawns is ignored for rendering (verified server-side: spawn z -88 with
// confirmed zone reloads rendered identically to stock), and the classic SHIP
// model floats with its hull bottom at the surface - riding ~4 units too high.
// Model-geometry shifts (DMSPRITEDEF2 CenterOffset) are re-floated away by the
// client, and a spawn-z write sticks but is never re-read for rendering (live
// dump 2026-07-19): the waterline the client computes is written to the boat's
// RENDER ACTOR position (z at actor+0x34, second copy +0x11C). This module
// re-asserts `actor z -= draft` on boat-race spawns each frame (change-
// detected, so it never accumulates), plus /rcpboat probe subcommands
// (dump/watch/setz/seta - same methodology as /hidecorpses debug).
// State persists to rof2ClientPlus.ini [Boat].
#pragma once

class RcpService;

class BoatDraft {
 public:
  explicit BoatDraft(RcpService *rcp);
  ~BoatDraft();

 private:
  RcpService *rcp_;
};

namespace boat_draft_settings {
float get_draft();
void set_draft(float units);
}  // namespace boat_draft_settings
