// rof2ClientPlus - additional key binds (Zeal binds port), stock RoF2.
//
// See keybinds.h for the mechanism summary and PORTING_NOTES.md (Keybinds)
// for the full RE. All addresses are stock-RoF2 facts from eqlib + disasm.
//
// Bind inventory (only binds the stock client does NOT already have - it
// natively covers strafe, NPC/PC/corpse target cycling, map toggle, reply
// target, last-two-targets, bag open/close, and 10 hotbar pages):
//   Pet Attack / Back Off / Follow / Guard / Sit / Health / Hold
//   Auto Fire, Auto Inventory, Buy/Sell Stack (merchant), Assist, Loot Target,
//   Toggle Nameplate Colors / Con Colors / Hide Self.
#include "keybinds.h"
#include "rebase.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "logger.h"
#include "nameplate.h"
#include "rcp.h"

// ---------------------------------------------------------------------------
// Stock RoF2 addresses (eqlib offsets/eqgame.h + disasm, May 10 2013 build).
// ---------------------------------------------------------------------------
static char **const kBindList = reinterpret_cast<char **>(::Rcp::eqva(0xACBEE8));  // char*[479] mappable-command names (.data)
static constexpr int kNumNamedCommands = 479;                         // dense 0..478; 479-499 are dispatch-only
static const uintptr_t kExecuteCmd = ::Rcp::eqva(0x4D7230);                    // __cdecl ExecuteCmd(cmd, keydown, data, combo)
static const uintptr_t kInitKeyboardAssignments = ::Rcp::eqva(0x7046C0);       // __thiscall COptionsWnd::InitKeyboardAssignments
static void **const kKeypressHandler = reinterpret_cast<void **>(::Rcp::eqva(0xE639B0));  // KeypressHandler* singleton
static const uintptr_t kLoadAndSetKeymappings = ::Rcp::eqva(0x55B100);         // __thiscall KeypressHandler::LoadAndSetKeymappings
static const uintptr_t kResetKeysToEqDefaults = ::Rcp::eqva(0x5598D0);         // __thiscall KeypressHandler::ResetKeysToEqDefaults
static const uintptr_t kSaveKeymapping = ::Rcp::eqva(0x55AC50);                // __thiscall KeypressHandler::SaveKeymapping(cmd, KeyCombo&, map)
static const uintptr_t kDeleteAllKeymappings = ::Rcp::eqva(0x55B020);          // __thiscall KeypressHandler::DeleteAllKeymappings
// KeypressHandler layout: KeyCombo NormalKey[500] @+0, AltKey[500] @+0x7D0,
// char CommandState[500] @+0xFA0. KeyCombo = {u8 Alt, u8 Ctrl, u8 Shift, u8 DIK}.
static constexpr uintptr_t kAltKeyOffset = 0x7D0;
static void **const kCEverQuest = reinterpret_cast<void **>(::Rcp::eqva(0xE67CCC));  // CEverQuest* (pinstCEverQuest)
static void **const kSelf = reinterpret_cast<void **>(::Rcp::eqva(0xDD2630));        // local PlayerClient*
static constexpr int kOffName = 0xA4;                                   // PlayerBase::Name (char[0x40])
static uint8_t *const kCmdStates = reinterpret_cast<uint8_t *>(::Rcp::eqva(0xDCEF08));  // g_eqCommandStates[500] (0 = suppressed)

// The stock Options window Keys tab data: COptionsWnd holds an 8-byte-stride
// {CXStr desc, int category} array at this+0x40C indexed by command id (dense,
// capacity exactly 479), filled inline by InitKeyboardAssignments. We rewrite
// our hijacked rows after the original runs. The key-capture path only allows
// command ids <= 463 (cmp at 0x70ACCB), which our block satisfies.
static constexpr uintptr_t kOptionsBindsOffset = 0x40C;
static const uintptr_t kCXStrAssignCharPtr = ::Rcp::eqva(0x805DE0);  // __thiscall CXStr::operator=(const char*)
static void **const kOptionsWnd = reinterpret_cast<void **>(::Rcp::eqva(0xD1FC6C));  // COptionsWnd* (null until built)
static const uintptr_t kRefreshKeyboardAssignmentList = ::Rcp::eqva(0x70A560);   // __thiscall COptionsWnd::Refresh...List()

// Keys-tab category BITMASKS (filter combobox: bit i == choice i; "All" =
// 0x7FFFF). Disasm of 0x7046C0 / the filter test at 0x70A6B4.
static constexpr int kCatCommands = 0x2;
static constexpr int kCatTarget = 0x8;
static constexpr int kCatUI = 0x40;

// eqclient.ini [KeyMaps] persistence (native format the client reads/writes):
// KEYMAPPING_<BINDLISTNAME>_<1|2> = int(dik | alt<<28 | ctrl<<29 | shift<<30).
static constexpr char kKeyMapsSection[] = "KeyMaps";

// Merchant full-stack buy/sell (all disasm-verified; see PORTING_NOTES.md).
// CQuantityWnd::Open reads the wnd-manager shift flag and, when set, commits
// the max quantity immediately without showing the dialog - the same path a
// native shift-click takes - so we fake shift and click the buy/sell handler.
static void **const kMerchantWnd = reinterpret_cast<void **>(::Rcp::eqva(0xD1FCA4));      // CMerchantWnd* (null until built)
static void **const kActiveMerchant = reinterpret_cast<void **>(::Rcp::eqva(0xDD264C));   // entity we are trading with
static void **const kWndManager = reinterpret_cast<void **>(::Rcp::eqva(0x15D3D00));      // CXWndManager*
static constexpr int kOffWndMgrShift = 0x9D;      // CXWndManager bool: shift held
static constexpr int kOffMerchantLocation = 0x23C;  // ItemGlobalIndex.Location of the selected slot
static constexpr int kOffMerchantSelItem = 0x248;   // ItemPtr pSelectedItem
static constexpr int kLocMerchant = 9;   // eItemContainerMerchant -> buying
static constexpr int kLocPossessions = 0;  // eItemContainerPossessions -> selling
static const uintptr_t kHandleBuy = ::Rcp::eqva(0x6F2620);   // __thiscall CMerchantWnd::HandleBuy(int qty; -1 = quantity flow)
static const uintptr_t kHandleSell = ::Rcp::eqva(0x6F29C0);  // __thiscall CMerchantWnd::HandleSell(int qty)

// Hijacked command indices: the CMD_REAL_ESTATE_* block (429-446) is the RoF2
// housing UI, absent on emu servers, and sits below the options page's <=463
// assignment bound - so these rows get native capture/persistence/UI slots.
enum RcpCmd {
  kCmdPetAttack = 429,
  kCmdPetBack = 430,
  kCmdPetFollow = 431,
  kCmdPetGuard = 432,
  kCmdPetSit = 433,
  kCmdPetHealth = 434,
  kCmdPetHold = 435,
  kCmdAutoFire = 436,
  kCmdAutoInventory = 437,
  kCmdBuySellStack = 438,
  kCmdAssist = 439,
  kCmdNpColors = 440,
  kCmdNpConColors = 441,
  kCmdNpHideSelf = 442,
  kCmdLootTarget = 443,
};
static constexpr int kFirstHijacked = 429;
static constexpr int kLastHijacked = 443;

// ---------------------------------------------------------------------------
// Bind table.
// ---------------------------------------------------------------------------
struct RcpBind {
  int cmd;                     // hijacked client command index
  const char *bind_name;       // BindList replacement -> ini key KEYMAPPING_<name>_N
  const char *label;           // stock options window Keys tab row text
  int category;                // Keys tab filter category
  void (*action)(int keydown); // called from the ExecuteCmd detour
};

static void action_pet(int keydown);          // fwd decls; table below dispatches by cmd
static void action_slash(int keydown);
static void action_buysell(int keydown);
static void action_nameplate(int keydown);

// One dispatcher per flavor keeps the table readable; each looks up the
// pressed cmd from g_active_cmd (set by the ExecuteCmd detour).
static int g_active_cmd = 0;

static const RcpBind kBinds[] = {
    {kCmdPetAttack, "RCP_PET_ATTACK", "Pet Attack", kCatCommands, action_pet},
    {kCmdPetBack, "RCP_PET_BACK_OFF", "Pet Back Off", kCatCommands, action_pet},
    {kCmdPetFollow, "RCP_PET_FOLLOW", "Pet Follow", kCatCommands, action_pet},
    {kCmdPetGuard, "RCP_PET_GUARD", "Pet Guard", kCatCommands, action_pet},
    {kCmdPetSit, "RCP_PET_SIT", "Pet Sit", kCatCommands, action_pet},
    {kCmdPetHealth, "RCP_PET_HEALTH", "Pet Health", kCatCommands, action_pet},
    {kCmdPetHold, "RCP_PET_HOLD", "Pet Hold", kCatCommands, action_pet},
    {kCmdAutoFire, "RCP_AUTO_FIRE", "Auto Fire", kCatCommands, action_slash},
    {kCmdAutoInventory, "RCP_AUTO_INVENTORY", "Auto Inventory", kCatCommands, action_slash},
    {kCmdBuySellStack, "RCP_BUY_SELL_STACK", "Buy/Sell Stack", kCatUI, action_buysell},
    {kCmdAssist, "RCP_ASSIST", "Assist", kCatTarget, action_slash},
    {kCmdNpColors, "RCP_NAMEPLATE_COLORS", "Toggle Nameplate Colors", kCatTarget, action_nameplate},
    {kCmdNpConColors, "RCP_NAMEPLATE_CON_COLORS", "Toggle Nameplate Con Colors", kCatTarget, action_nameplate},
    {kCmdNpHideSelf, "RCP_NAMEPLATE_HIDE_SELF", "Toggle Hide Own Nameplate", kCatTarget, action_nameplate},
    {kCmdLootTarget, "RCP_LOOT_TARGET", "Loot Target", kCatCommands, action_slash},
};
static constexpr int kNumBinds = sizeof(kBinds) / sizeof(kBinds[0]);

static const RcpBind *find_bind(unsigned int cmd) {
  if (cmd < (unsigned)kFirstHijacked || cmd > (unsigned)kLastHijacked) return nullptr;
  for (const auto &b : kBinds)
    if ((unsigned)b.cmd == cmd) return &b;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Settings.
// ---------------------------------------------------------------------------
static bool g_per_character = false;  // [Binds] PerCharacter in rof2ClientPlus.ini
static constexpr char kIniSection[] = "Binds";

// Which character the per-character overlay was last applied for; cleared to
// force a reload on the next in-game frame (world entry, char switch, resets).
static char g_overlay_char[0x40] = {0};

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "PerCharacter")) g_per_character = ini.getValue<bool>(kIniSection, "PerCharacter");
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "PerCharacter", g_per_character);
}

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------
static const char *self_name() {
  char *self = static_cast<char *>(*kSelf);
  if (!self) return nullptr;
  const char *name = self + kOffName;
  return name[0] ? name : nullptr;
}

// Per-character section: "KeyMaps_<CharName>" in eqclient.ini.
static std::string per_char_section() {
  const char *name = self_name();
  return name ? (std::string(kKeyMapsSection) + "_" + name) : std::string();
}

static std::string keymap_ini_key(int cmd, int map) {
  return std::string("KEYMAPPING_") + kBindList[cmd] + "_" + (map == 0 ? "1" : "2");
}

// Forward a slash command through the client's interpreter (the trampoline of
// our own InterpretCmd hook, bypassing our detour - all forwards are native
// commands). RoF2 pointers, not the vendored TAKP ForwardCommand.
typedef void(__fastcall *InterpretCmd_t)(void *cevq, int unused_edx, void *player, const char *cmd);
static void forward_slash_command(const char *cmd) {
  RcpService *rcp = RcpService::get_instance();
  auto it = rcp->hooks->hook_map.find("commands");
  if (it == rcp->hooks->hook_map.end()) return;
  void *cevq = *kCEverQuest;
  void *self = *kSelf;
  if (!cevq || !self) return;
  it->second->original((InterpretCmd_t) nullptr)(cevq, 0, self, cmd);
}

typedef void(__fastcall *ThisCall0_t)(void *self, int unused_edx);
static void call_thiscall0(uintptr_t addr, void *self) { reinterpret_cast<ThisCall0_t>(addr)(self, 0); }

// KeyCombo <-> the int format the client persists ({u8 Alt,Ctrl,Shift,DIK}).
static uint32_t combo_bytes_from_int(int v) {
  uint8_t alt = (v >> 28) & 1, ctrl = (v >> 29) & 1, shift = (v >> 30) & 1, key = v & 0xFF;
  return (uint32_t)alt | ((uint32_t)ctrl << 8) | ((uint32_t)shift << 16) | ((uint32_t)key << 24);
}
static int combo_int_from_bytes(uint32_t bytes) {
  int alt = bytes & 1, ctrl = (bytes >> 8) & 1, shift = (bytes >> 16) & 1, key = (bytes >> 24) & 0xFF;
  return key | (alt << 28) | (ctrl << 29) | (shift << 30);
}

static std::string combo_text(uint32_t bytes) {
  if (!bytes) return "<none>";
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%s%s%sDIK 0x%02X", (bytes & 1) ? "ALT+" : "", ((bytes >> 8) & 1) ? "CTRL+" : "",
                ((bytes >> 16) & 1) ? "SHIFT+" : "", (bytes >> 24) & 0xFF);
  return buf;
}

// ---------------------------------------------------------------------------
// Keymap load / per-character overlay.
// ---------------------------------------------------------------------------
static uint32_t *normal_key(void *handler, int cmd) {
  return reinterpret_cast<uint32_t *>(static_cast<char *>(handler) + cmd * 4);
}
static uint32_t *alt_key(void *handler, int cmd) {
  return reinterpret_cast<uint32_t *>(static_cast<char *>(handler) + kAltKeyOffset + cmd * 4);
}

// Overlay [KeyMaps_<Char>] entries on top of the current (global) keymaps.
static void apply_per_char_overlay(void *handler) {
  std::string section = per_char_section();
  if (section.empty()) return;
  IO_ini ini(IO_ini::kClientFilename);
  int applied = 0;
  for (int cmd = 0; cmd < kNumNamedCommands; ++cmd) {
    if (!kBindList[cmd]) continue;
    for (int map = 0; map < 2; ++map) {
      std::string key = keymap_ini_key(cmd, map);
      if (!ini.exists(section, key)) continue;
      int v = ini.getValue<int>(section, key);
      *(map == 0 ? normal_key(handler, cmd) : alt_key(handler, cmd)) = combo_bytes_from_int(v);
      ++applied;
    }
  }
  logger::logf("[binds] per-character overlay '%s': %d entries applied", section.c_str(), applied);
}

// Rebuild the live keymaps: client defaults -> global [KeyMaps] (with our
// renamed entries) -> optional per-character overlay.
static void reload_keymaps(bool reset_defaults) {
  void *handler = *kKeypressHandler;
  if (!handler) return;
  if (reset_defaults) call_thiscall0(kResetKeysToEqDefaults, handler);
  call_thiscall0(kLoadAndSetKeymappings, handler);
  if (g_per_character && Rcp::Game::is_in_game()) apply_per_char_overlay(handler);
}

// ---------------------------------------------------------------------------
// Bind actions.
// ---------------------------------------------------------------------------
static void action_pet(int keydown) {
  if (!keydown) return;
  const char *sub = nullptr;
  switch (g_active_cmd) {
    case kCmdPetAttack: sub = "/pet attack"; break;
    case kCmdPetBack: sub = "/pet back off"; break;
    case kCmdPetFollow: sub = "/pet follow"; break;
    case kCmdPetGuard: sub = "/pet guard"; break;
    case kCmdPetSit: sub = "/pet sit"; break;
    case kCmdPetHealth: sub = "/pet health"; break;
    case kCmdPetHold: sub = "/pet hold"; break;
    default: return;
  }
  forward_slash_command(sub);
}

static void action_slash(int keydown) {
  if (!keydown) return;
  switch (g_active_cmd) {
    case kCmdAutoFire: forward_slash_command("/autofire"); break;
    case kCmdAutoInventory: forward_slash_command("/autoinventory"); break;
    case kCmdAssist: forward_slash_command("/assist"); break;
    case kCmdLootTarget: forward_slash_command("/loot"); break;
    default: break;
  }
}

typedef void(__fastcall *MerchantHandle_t)(void *merchant_wnd, int unused_edx, int qty);

static void action_buysell(int keydown) {
  if (!keydown) return;
  void *mw = *kMerchantWnd;
  if (!mw || !*kActiveMerchant) return;  // Merchant window never built / not trading.
  if (!*reinterpret_cast<void **>(static_cast<char *>(mw) + kOffMerchantSelItem)) return;  // No item selected.
  char *mgr = static_cast<char *>(*kWndManager);
  if (!mgr) return;
  const int loc = *reinterpret_cast<int *>(static_cast<char *>(mw) + kOffMerchantLocation);
  if (loc != kLocMerchant && loc != kLocPossessions) return;

  // Fake shift through HandleBuy/HandleSell(-1): the client computes the full
  // stack itself (min of merchant stock and stack size / current stack count),
  // runs its own money+space checks, and commits without the quantity dialog.
  // The whole flow is synchronous, so restoring the flag right after is safe.
  const char old_shift = mgr[kOffWndMgrShift];
  mgr[kOffWndMgrShift] = 1;
  reinterpret_cast<MerchantHandle_t>(loc == kLocMerchant ? kHandleBuy : kHandleSell)(mw, 0, -1);
  mgr[kOffWndMgrShift] = old_shift;
}

static void action_nameplate(int keydown) {
  if (!keydown) return;
  bool con = nameplate_settings::get_con_colors();
  bool colors = nameplate_settings::get_state_colors();
  bool hide = nameplate_settings::get_hide_self();
  switch (g_active_cmd) {
    case kCmdNpColors: colors = !colors; break;
    case kCmdNpConColors: con = !con; break;
    case kCmdNpHideSelf: hide = !hide; break;
    default: return;
  }
  nameplate_settings::set(con, colors, nameplate_settings::get_target_color(), nameplate_settings::get_target_blink(),
                          nameplate_settings::get_target_marker(), nameplate_settings::get_target_health(), hide);
}

// ---------------------------------------------------------------------------
// Detours.
// ---------------------------------------------------------------------------
// ExecuteCmd is __cdecl; keyboard call sites push 3 args, some UI paths push a
// 4th (the KeyCombo) - forwarding all 4 reproduces both shapes exactly (the
// callee only reads the 4th on paths whose callers pass it).
typedef int(__cdecl *ExecuteCmd_t)(unsigned int cmd, int keydown, int data, int combo);

static int __cdecl ExecuteCmd_hk(unsigned int cmd, int keydown, int data, int combo) {
  const RcpBind *bind = find_bind(cmd);
  if (bind) {
    if (!kCmdStates[cmd]) return 0;  // Mirror the native command-suppression gate.
    // Swallow the dead real-estate handler entirely; run our action instead.
    // Native UI routing already keeps keys away from binds while typing (the
    // CXWndManager eats them before HandleKeyDown scans the keymaps).
    rcp_guard::run("binds.execute", [&] {
      g_active_cmd = (int)cmd;
      bind->action(keydown);
    });
    return 1;
  }
  RcpService *rcp = RcpService::get_instance();
  return rcp->hooks->hook_map["binds_execute_cmd"]->original((ExecuteCmd_t) nullptr)(cmd, keydown, data, combo);
}

// COptionsWnd::InitKeyboardAssignments fills the Keys-tab rows; rewrite the
// hijacked rows' label + category after the original has populated them.
typedef void(__fastcall *InitKeyboardAssignments_t)(void *options_wnd, int unused_edx);

typedef void *(__fastcall *CXStrAssign_t)(void *cxstr, int unused_edx, const char *text);

static void fill_options_rows(void *options_wnd) {
  for (const auto &b : kBinds) {
    char *entry = static_cast<char *>(options_wnd) + kOptionsBindsOffset + b.cmd * 8;
    reinterpret_cast<CXStrAssign_t>(kCXStrAssignCharPtr)(entry, 0, b.label);  // CXStr desc @ +0
    *reinterpret_cast<int *>(entry + 4) = b.category;
  }
}

// Repaint the on-screen Keys list if the options window exists (it rereads the
// row combos from the KeypressHandler, so call after any keymap reload).
static void refresh_options_list() {
  void *opts = *kOptionsWnd;
  if (opts) call_thiscall0(kRefreshKeyboardAssignmentList, opts);
}

static void __fastcall InitKeyboardAssignments_hk(void *options_wnd, int unused_edx) {
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["binds_init_kb_assign"]->original((InitKeyboardAssignments_t) nullptr)(options_wnd, unused_edx);
  rcp_guard::run("binds.optinit", [&] { fill_options_rows(options_wnd); });
}

// KeypressHandler::SaveKeymapping(cmd, const KeyCombo&, map): in per-character
// mode redirect the write to [KeyMaps_<Char>] so global binds stay untouched.
typedef void(__fastcall *SaveKeymapping_t)(void *handler, int unused_edx, unsigned int cmd, uint32_t *combo, int map);

static void __fastcall SaveKeymapping_hk(void *handler, int unused_edx, unsigned int cmd, uint32_t *combo, int map) {
  if (g_per_character && Rcp::Game::is_in_game() && cmd < (unsigned)kNumNamedCommands && kBindList[cmd]) {
    std::string section = per_char_section();
    if (!section.empty()) {
      bool ok = false;
      rcp_guard::run("binds.save", [&] {
        IO_ini ini(IO_ini::kClientFilename);
        ini.setValue<int>(section, keymap_ini_key((int)cmd, map), combo_int_from_bytes(*combo));
        ok = true;
      });
      if (ok) return;  // Redirected; skip the global write.
    }
  }
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["binds_save_keymap"]->original((SaveKeymapping_t) nullptr)(handler, unused_edx, cmd, combo, map);
}

// KeypressHandler::DeleteAllKeymappings ("Reload defaults" in the options
// window): in per-character mode wipe only this character's section.
typedef void(__fastcall *DeleteAllKeymappings_t)(void *handler, int unused_edx);

static void __fastcall DeleteAllKeymappings_hk(void *handler, int unused_edx) {
  if (g_per_character && Rcp::Game::is_in_game()) {
    std::string section = per_char_section();
    if (!section.empty()) {
      IO_ini ini(IO_ini::kClientFilename);
      ini.deleteSection(section);
      // Force a next-frame reload so the live keymaps land on the GLOBAL binds
      // (the client's own follow-up reset leaves raw defaults otherwise).
      g_overlay_char[0] = 0;
      logger::logf("[binds] per-character reset: deleted [%s]", section.c_str());
      return;  // Global [KeyMaps] stays untouched.
    }
  }
  RcpService *rcp = RcpService::get_instance();
  rcp->hooks->hook_map["binds_delete_keymaps"]->original((DeleteAllKeymappings_t) nullptr)(handler, unused_edx);
}

// ---------------------------------------------------------------------------
// Settings accessors + per-frame world-entry watcher.
// ---------------------------------------------------------------------------
namespace keybind_settings {
bool get_per_character() { return g_per_character; }
void set_per_character(bool enable) {
  if (g_per_character == enable) return;
  g_per_character = enable;
  save_settings();
  if (Rcp::Game::is_in_game()) {
    reload_keymaps(true);  // Re-sync live keymaps immediately.
    refresh_options_list();
  }
}
}  // namespace keybind_settings

// Track world entry / character switches so the per-character overlay applies
// with the right name. Cheap: two global reads per frame until in-game.
void KeyBinds::on_frame() {
  if (!g_per_character) return;
  if (!Rcp::Game::is_in_game()) {
    g_overlay_char[0] = 0;  // Re-apply on the next world entry.
    return;
  }
  const char *name = self_name();
  if (!name || std::strcmp(name, g_overlay_char) == 0) return;
  std::snprintf(g_overlay_char, sizeof(g_overlay_char), "%s", name);
  rcp_guard::run("binds.overlay", [&] {
    reload_keymaps(true);
    refresh_options_list();
  });
  Rcp::Game::print_chat("rof2ClientPlus: per-character keybinds loaded for %s.", name);
}

// ---------------------------------------------------------------------------
// Status + command.
// ---------------------------------------------------------------------------
static void print_status() {
  Rcp::Game::print_chat("rof2ClientPlus binds: per-character %s. Assign keys in Options -> Keys.",
                        g_per_character ? "ON" : "OFF");
}

static void print_bind_list() {
  void *handler = *kKeypressHandler;
  for (const auto &b : kBinds) {
    uint32_t nk = handler ? *normal_key(handler, b.cmd) : 0;
    uint32_t ak = handler ? *alt_key(handler, b.cmd) : 0;
    Rcp::Game::print_chat("  %s: %s | alt %s", b.label, combo_text(nk).c_str(), combo_text(ak).c_str());
  }
}

// ---------------------------------------------------------------------------
// Construction: patch BindList, install detours, reload persisted keys.
// ---------------------------------------------------------------------------
KeyBinds::KeyBinds(RcpService *rcp) : rcp_(rcp) {
  load_settings();

  // Repoint the dead commands' names so the client's own ini save/load and
  // options-window key capture operate on our binds. Plain .data writes.
  for (const auto &b : kBinds) kBindList[b.cmd] = const_cast<char *>(b.bind_name);

  rcp->hooks->Add("binds_execute_cmd", kExecuteCmd, ExecuteCmd_hk, hook_type_detour);
  rcp->hooks->Add("binds_init_kb_assign", kInitKeyboardAssignments, InitKeyboardAssignments_hk, hook_type_detour);
  rcp->hooks->Add("binds_save_keymap", kSaveKeymapping, SaveKeymapping_hk, hook_type_detour);
  rcp->hooks->Add("binds_delete_keymaps", kDeleteAllKeymappings, DeleteAllKeymappings_hk, hook_type_detour);

  // The client's boot-time LoadAndSetKeymappings ran before our rename, using
  // the old real-estate names. Clear any stale combos it may have put in our
  // hijacked slots, then re-drive the load so KEYMAPPING_RCP_* entries apply.
  if (void *handler = *kKeypressHandler) {
    for (const auto &b : kBinds) {
      *normal_key(handler, b.cmd) = 0;
      *alt_key(handler, b.cmd) = 0;
    }
    call_thiscall0(kLoadAndSetKeymappings, handler);
  }

  rcp->commands_hook->Add(
      "/rcpbinds", {},
      "Extra key binds: '/rcpbinds list', '/rcpbinds perchar on|off' (per-character keybinds), '/rcpbinds reload'.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2 && args[1] == "list") {
          print_bind_list();
          return true;
        }
        if (args.size() >= 3 && args[1] == "perchar") {
          keybind_settings::set_per_character(args[2] == "on" || args[2] == "1");
          print_status();
          return true;
        }
        if (args.size() >= 2 && args[1] == "reload") {
          reload_keymaps(true);
          refresh_options_list();
          Rcp::Game::print_chat("rof2ClientPlus: keybinds reloaded from eqclient.ini.");
          return true;
        }
        print_status();
        return true;
      });

  logger::logf("[binds] installed: %d custom binds on cmd %d-%d, per-character=%d", kNumBinds, kFirstHijacked,
               kLastHijacked, (int)g_per_character);
}
