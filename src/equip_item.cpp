// rof2ClientPlus - right-click to equip (RoF2-native). See equip_item.h.
#include "equip_item.h"
#include "rebase.h"

#include <windows.h>

#include <cstdint>

#include <string>
#include <vector>

#include "commands.h"
#include "game_functions.h"  // Rcp::Game::print_chat
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"
#include "spellbook_ui.h"  // spellbook_scribe: right-click a scroll -> auto-scribe

namespace {

constexpr char kIniSection[] = "EquipItem";
bool g_enabled = false;

// ---- stock RoF2 addresses/offsets (eqlib, build 2013-05-10 == ours; verified clean prologues) ----
// void __thiscall CInvSlot::HandleRButtonUp(const CXPoint& pt).
const int kCInvSlot_HandleRButtonUp = ::Rcp::eqva(0x697250);
// ItemPtr __thiscall CInvSlot::GetItem(); returns an 8-byte SoeUtil::SharedPtr by hidden return ptr.
const int kCInvSlot_GetItem = ::Rcp::eqva(0x694780);
// bool __thiscall CInvSlotMgr::MoveItem(const ItemGlobalIndex& from, const ItemGlobalIndex& to,
//                                       bool bDebugOut, bool CombineIsOk, bool MoveFromIntoToBag, bool MoveToIntoFromBag).
const int kCInvSlotMgr_MoveItem = ::Rcp::eqva(0x698D80);
void **const kInvSlotMgr = reinterpret_cast<void **>(::Rcp::eqva(0xD1FD80));  // pinstCInvSlotMgr

// pinstCXWndManager + its cached keyboard-flag bytes (read by GetKeyboardFlags): /*0x9d*/ Shift,
// /*0x9e*/ Ctrl, /*0x9f*/ LAlt, /*0xa0*/ RAlt. We read the Alt bytes to tell an equip (Alt) from a
// plain right-click (which lets a clicky cast natively).
void **const kCXWndManager = reinterpret_cast<void **>(::Rcp::eqva(0x15D3D00));
constexpr int kOffKb_LAlt = 0x9f;
constexpr int kOffKb_RAlt = 0xa0;

// CInvSlot layout: /*0x04*/ CInvSlotWnd* pInvSlotWnd; /*0x10*/ bool bEnabled.
constexpr int kOffInvSlot_Wnd = 0x04;
// CInvSlotWnd: /*0x264*/ ItemGlobalIndex ItemLocation { int Location; short slots[3]; }.
constexpr int kOffInvSlotWnd_ItemLocation = 0x264;
// ItemClient: /*0x144*/ ItemDefinitionPtr SharedItemDef -> SharedPtr{ ItemDefinition* pObject; ... }.
// (NOT ItemBase::Item1@0x9c, which is null on ItemClient - the def lives in the derived class.)
constexpr int kOffItem_Definition = 0x144;
// ItemPtr is VePointer<ItemClient> (intrusive). ItemBase's first base is VeBaseReferenceCount
// { vtable@0x0; int ReferenceCount@0x4 }, so the refcount lives at item+0x4.
constexpr int kOffItem_RefCount = 0x04;
// ItemDefinition: /*0xf0*/ int EquipSlots (bitmask, 1<<InvSlotIndex). Note: offset 0x00 is Name[],
// NOT a type field - non-wearables are filtered by EquipSlots == 0 (bags/books have no equip slots).
constexpr int kOffDef_EquipSlots = 0xf0;
// ItemDefinition: /*0x284*/ ItemSpellData SpellData -> Spells[ItemSpellType_Clicky=0].SpellID @ +0x00.
// A right-click-activatable item (clicky/mount/illusion/etc.) has SpellID > 0 in that first slot.
constexpr int kOffDef_ClickySpellID = 0x284;

// ItemContainerInstance / worn slot numbering (eqlib eInventorySlot).
constexpr int kLocPossessions = 0;
constexpr int kInvSlot_FirstBag = 23;  // InvSlot_Bag1 (general-inventory area 23..32)
constexpr int kInvSlot_LastBag = 32;   // InvSlot_Bag10

// A stack-friendly mirror of eqlib's ItemGlobalIndex (0x0c bytes): { int Location; short slots[3]; }.
struct GlobalIndex {
  int location;
  short slots[3];
  short pad;  // pad out to 0x0c so &this matches the client's struct footprint.
};

// Equip-slot fill priority (RoF2 eInventorySlot indices 0..22): weapons/range, then armor, then
// jewelry, charm/power-source, ammo. First matching slot the item can go in wins (v1: first-fit).
constexpr int kEquipPriorityOrder[] = {
    13, 14, 11,             // Primary, Secondary, Range
    17, 18, 2, 7, 12, 19, 6, 8,  // Chest, Legs, Head, Arms, Hands, Feet, Shoulders, Back
    5, 3, 20, 9, 10, 1, 4, 15, 16,  // Neck, Face, Waist, LeftWrist, RightWrist, LeftEar, RightEar, LeftRing, RightRing
    0, 21, 22,              // Charm, PowerSource, Ammo
};

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "RightClickToEquip")) g_enabled = ini.getValue<bool>(kIniSection, "RightClickToEquip");
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "RightClickToEquip", g_enabled);
}

// Read the clicked slot's source ItemGlobalIndex from its CInvSlotWnd (pure reads).
bool read_slot_location(void *inv_slot, GlobalIndex *out) {
  if (!inv_slot) return false;
  void *wnd = *reinterpret_cast<void **>(reinterpret_cast<char *>(inv_slot) + kOffInvSlot_Wnd);
  if (!wnd) return false;
  char *loc = reinterpret_cast<char *>(wnd) + kOffInvSlotWnd_ItemLocation;
  out->location = *reinterpret_cast<int *>(loc);
  out->slots[0] = *reinterpret_cast<short *>(loc + 4);
  out->slots[1] = *reinterpret_cast<short *>(loc + 6);
  out->slots[2] = *reinterpret_cast<short *>(loc + 8);
  out->pad = -1;
  return true;
}

// Fetch the raw ItemClient* for the clicked slot via CInvSlot::GetItem. GetItem returns an
// ItemPtr (VePointer<ItemClient>) by value, whose copy-construct bumped the intrusive refcount; we
// grab the raw pointer and immediately drop that extra reference (the inventory keeps its own, so the
// item stays alive for our synchronous use). Balancing it here avoids a per-equip leak.
void *get_slot_item(void *inv_slot) {
  void *retbuf[2] = {nullptr, nullptr};  // VePointer<ItemClient>: retbuf[0] is the raw ItemClient*.
  reinterpret_cast<void *(__thiscall *)(void *, void *)>(kCInvSlot_GetItem)(inv_slot, retbuf);
  void *item = retbuf[0];
  if (item)
    InterlockedDecrement(reinterpret_cast<volatile LONG *>(reinterpret_cast<char *>(item) + kOffItem_RefCount));
  return item;
}

// Equip the wearable item in `inv_slot` (whose location is `src`) into the best worn slot. When
// `skip_clicky` is set, a right-click-activatable item is left for the client's native handler (so a
// plain right-click casts the clicky instead of equipping it). Returns true if we handled it (absorb),
// false to let the client's native right-click run.
bool try_equip(void *inv_slot, const GlobalIndex &src, bool skip_clicky) {
  // Only equip from the general-inventory area (bags + their contents / a general slot). Worn slots
  // (0..22) are already equipped; the bag itself / non-wearables fall through below.
  if (src.slots[0] < kInvSlot_FirstBag || src.slots[0] > kInvSlot_LastBag) return false;

  void *item = get_slot_item(inv_slot);
  void *def = item ? *reinterpret_cast<void **>(reinterpret_cast<char *>(item) + kOffItem_Definition) : nullptr;
  int equip_slots = def ? *reinterpret_cast<int *>(reinterpret_cast<char *>(def) + kOffDef_EquipSlots) : 0;
  if (!item || !def || equip_slots == 0) return false;  // Not a wearable item -> client handles it.

  if (skip_clicky && *reinterpret_cast<int *>(reinterpret_cast<char *>(def) + kOffDef_ClickySpellID) > 0)
    return false;  // Clicky item on a plain right-click -> let the client cast it (Alt+click equips).

  // Pick the first slot in priority order the item can be worn in.
  int dst_slot = -1;
  for (int s : kEquipPriorityOrder) {
    if (equip_slots & (1 << s)) {
      dst_slot = s;
      break;
    }
  }
  if (dst_slot < 0) return false;

  void *mgr = *kInvSlotMgr;
  if (!mgr) return false;

  GlobalIndex dst = {kLocPossessions, {static_cast<short>(dst_slot), -1, -1}, -1};
  reinterpret_cast<bool(__thiscall *)(void *, const void *, const void *, bool, bool, bool, bool)>(kCInvSlotMgr_MoveItem)(
      mgr, &src, &dst, false, true, false, false);
  return true;  // Absorb: we handled the right-click.
}

// Detour: void __thiscall CInvSlot::HandleRButtonUp(this, const CXPoint& pt).
//   plain right-click on a spell scroll in a bag  -> scribe it (spellbook_scribe, /rcpscribe)
//   plain right-click on a wearable bag item      -> equip it, UNLESS it is a clicky (then the client
//                                                    casts it, as it does natively)
//   Alt + right-click on a wearable bag item      -> equip it (equips clickies too; skips the scribe)
//   everything else                               -> the client's native right-click, unchanged
// This detour is the single owner of the HandleRButtonUp hook - the scribe feature plugs in here
// instead of adding a second detour on the same address.
void __fastcall CInvSlot_HandleRButtonUp(void *thiz, int unused_edx, const void *pt) {
  auto call_original = [&] {
    RcpService::get_instance()->hooks->hook_map["equip_rbtn"]->original(CInvSlot_HandleRButtonUp)(thiz, unused_edx, pt);
  };

  GlobalIndex src;
  void *wmgr = *kCXWndManager;
  if (wmgr && (g_enabled || spellbook_scribe::get_enabled()) && read_slot_location(thiz, &src) &&
      src.location == kLocPossessions && src.slots[0] >= kInvSlot_FirstBag && src.slots[0] <= kInvSlot_LastBag) {
    bool alt = *(reinterpret_cast<uint8_t *>(wmgr) + kOffKb_LAlt) || *(reinterpret_cast<uint8_t *>(wmgr) + kOffKb_RAlt);
    // Scribe first: it only claims unscribed spell scrolls (never wearable, so the equip
    // path below cannot shadow it); anything else falls through.
    if (!alt && spellbook_scribe::get_enabled() &&
        spellbook_scribe::handle_inventory_rclick(get_slot_item(thiz), src.location, src.slots))
      return;
    // Alt+right-click equips anything (incl. clickies); plain right-click equips non-clickies but lets
    // the client cast a clicky. In both cases a non-wearable falls through to the native handler.
    if (g_enabled && try_equip(thiz, src, /*skip_clicky=*/!alt)) return;
  }
  call_original();
}

}  // namespace

namespace equip_item_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  save_settings();
}
}  // namespace equip_item_settings

EquipItem::EquipItem(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  rcp->hooks->Add("equip_rbtn", kCInvSlot_HandleRButtonUp, CInvSlot_HandleRButtonUp, hook_type_detour);

  rcp->commands_hook->Add(
      "/rcpequip", {},
      "Right-click to equip: '/rcpequip' toggles; 'on|off'. When on, right-click a wearable item in a "
      "bag to auto-equip it into the best slot.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1")
            g_enabled = true;
          else if (a == "off" || a == "0")
            g_enabled = false;
          else {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpequip on|off'");
            return true;
          }
        } else {
          g_enabled = !g_enabled;
        }
        save_settings();
        Rcp::Game::print_chat("rof2ClientPlus right-click-to-equip: %s", g_enabled ? "ON" : "OFF");
        return true;
      });

  logger::logf("[equip] EquipItem constructed (RoF2-native); enabled=%d; hook @0x%x; /rcpequip registered",
               (int)g_enabled, kCInvSlot_HandleRButtonUp);
}
