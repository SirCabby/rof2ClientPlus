// rof2ClientPlus - additional key binds (Zeal binds port), stock RoF2.
//
// Adds mappable key binds the stock client lacks (pet commands, autofire,
// auto-inventory, merchant stack buy/sell, assist, loot target, nameplate
// toggles) and optional per-character keybind storage. Self-contained with raw stock-RoF2
// addresses (chase_cam.cpp / nameplate.cpp style); the vendored binds.cpp is a
// non-portable TAKP reference and stays unconstructed.
//
// Mechanism (RoF2 differs fundamentally from TAKP - see PORTING_NOTES.md):
// the client's mappable-command table (BindList, 479 dense entries) has no
// free named slots and its key arrays are fixed at 500, so instead of
// appending new indices we HIJACK the dead CMD_REAL_ESTATE_* command block
// (indices 429+, the housing UI that does not exist on emu servers):
//  - BindList[cmd] pointers (plain .data) are repointed to our names, which
//    gives native ini persistence (KEYMAPPING_RCP_*_N in eqclient.ini
//    [KeyMaps]) and native key capture for free.
//  - ExecuteCmd (0x4D7230) is detoured to swallow those indices and run our
//    callbacks instead of the dead real-estate handlers.
//  - COptionsWnd::InitKeyboardAssignments (0x7046C0) is detoured to rewrite
//    the hijacked rows' label + category in the stock Options window Keys tab.
// Per-character mode detours KeypressHandler::SaveKeymapping and overlays a
// [KeyMaps_<CharName>] section on top of the globals at world entry.
#pragma once

class RcpService;

class KeyBinds {
 public:
  KeyBinds(RcpService *rcp);

  // Called every frame from the main loop; watches for world entry (and
  // character changes) to apply the per-character keymap overlay.
  void on_frame();

 private:
  RcpService *rcp_ = nullptr;
};

// Accessors for the options-window UI / commands to read/write live settings.
namespace keybind_settings {
bool get_per_character();
void set_per_character(bool enable);  // Applies live (reloads keymaps) + persists.
}  // namespace keybind_settings
