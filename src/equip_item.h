// rof2ClientPlus - right-click to equip (RoF2-native reimplementation).
//
// Right-click a wearable item in an inventory bag and it auto-swaps into the best matching
// equipment slot. Unlike the vendored Zeal (TAKP) interface layer - whose game_functions/game_ui
// addresses do NOT match this RoF2 client (see PORTING_NOTES.md "vendored TAKP layer") - this module
// is fully self-contained with stock RoF2 addresses/offsets taken from the eqlib fact-reference,
// whose build (2013-05-10) is byte-identical to ours (verified: the offsets land on clean function
// prologues in our eqgame.exe).
//
// Mechanism: a detour on CInvSlot::HandleRButtonUp (@0x697250). When enabled and the clicked slot is
// a bag slot holding an equippable item, we resolve the source item's ItemGlobalIndex and drive the
// client's own CInvSlotMgr::MoveItem to swap it into the best equip slot, then absorb the right
// click. Otherwise we call the original so the client's native right-click (item inspect, use, open
// bag, etc.) runs unchanged - so this is purely additive and does nothing until '/rcpequip on'.
//
// Modifier scheme: plain right-click equips, but a CLICKY item is left to the client's native handler
// so it casts (the client already does this on a no-Alt right-click). Alt+right-click always equips,
// so a clicky can still be equipped with Alt.
//
// '/rcpequip [on|off]'. Ships OFF. Persisted to rof2ClientPlus.ini [EquipItem] RightClickToEquip.
#pragma once

class RcpService;

class EquipItem {
 public:
  explicit EquipItem(RcpService *rcp);

 private:
  RcpService *rcp_ = nullptr;
};

// Accessor for the options-window UI to read/write the live setting (persists to ini on set).
namespace equip_item_settings {
bool get_enabled();
void set_enabled(bool on);
}  // namespace equip_item_settings
