// rof2ClientPlus - Zeal-style spell book window (/rcpbook). See spellbook_ui.h.
//
// Everything here is driven by the per-frame poll (main thread, ProcessGameEvents),
// the same architecture as rcp_options_ui.cpp: the SIDL template arrives via the
// client's own UI load (standalone EQUI_RcpSpellBook.xml + an EQUI.xml Include),
// the window is instantiated lazily with CreateXWndFromTemplate, controls are
// found by name and polled, and every cached handle is dropped on any EQUI.xml
// (re)load or when not in game.
//
// Data sources (eqlib 2013-05-10 layouts; game/Spells.h, PcProfile.h, UI.h):
//  - Spell DB: ClientSpellManager* at *(0xE646B0); the ID->EQ_Spell* table is a
//    flat array at mgr+0x2c180 (45001 entries - the layout self-checks: table end
//    0x2c180+45001*4 == 0x580a4, exactly where eqlib places SpellExtraData).
//  - Character: pLocalPC *(0xDD261C) -> ProfileManager embedded at +0x31F0
//    {ProfileList* pFirst; int CurProfileList}; walk the ProfileList chain
//    (ListType@0, pFirst@4, pNext@0xc) to the current PcProfile. On it:
//    SpellBook int[720]@+0x2520, MemorizedSpells int[16]@+0x3060 (12 usable),
//    Class@+0x3374, Level@+0x3388.
//  - Strings: CDBStr::GetString@0x4866C0 on *(0xD1F380); type 5 = spell
//    category/subcategory names, type 6 = spell descriptions.
//  - Stock windows are found BY NAME through CDisplay's gameScreens registry
//    (CDisplay *(0xDD2660) + 0x2d84; every entry is a registered CSidlScreenWnd,
//    name = SidlText CXStr @wnd+0x1dc). CDisplay+0x2d78 (ActorClipPlane) is
//    already in-game-proven by view_distance.cpp, which pins this struct tail.
//
// Memorize = the stock book's own "pick up on cursor" flow, replicated from
// eqlib's CCursorAttachment::AttachSpellToCursor reference: clicking a spell's
// icon cell attaches the spell to the cursor (type eCursorAttachment_MemorizeSpell,
// index = spell book slot); dropping it on a cast-bar gem runs the client's
// native memorize (animation, server round-trip, gem refresh) untouched.
#include "spellbook_ui.h"
#include "rebase.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"

// commands.cpp helper (external linkage, no header): runs a STOCK chat command
// through the client's own InterpretCmd, bypassing the mod's command detour.
void ForwardCommand(std::string cmd);
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"
#include "spell_icons.h"

namespace {

constexpr char kIniSection[] = "SpellBook";
constexpr char kIniKeyEnabled[] = "NewWindow";
constexpr char kIniKeyUnscribed[] = "ShowUnscribed";
constexpr char kIniKeyScribe[] = "RightClickScribe";

// ---- stock RoF2 addresses (eqlib offsets/eqgame.h + repo-proven values) ----
const int kCreateXWnd = ::Rcp::eqva(0x870400);    // CSidlManagerBase::CreateXWndFromTemplate(parent,name)
const int kGetChildItem = ::Rcp::eqva(0x85CFD0);  // CSidlScreenWnd::GetChildItem(name, flag)
const int kSetFocus = ::Rcp::eqva(0x865250);      // CXWnd::SetFocus() - route typing to the search box on open
const int kCXStrCtor = ::Rcp::eqva(0x805C20);     // CXStr::CXStr(const char*)
const int kCXStrDtor = ::Rcp::eqva(0x465AE0);     // CXStr::~CXStr
void **const kSidlManager = reinterpret_cast<void **>(::Rcp::eqva(0x15D3D08));  // pinstCSidlManager
constexpr int kShowVtOffset = 0xD8;           // CXWnd vtable Show() slot
constexpr int kSetWindowTextVtOffset = 0x124; // CXWnd vtable SetWindowText(const CXStr&) slot
constexpr int kCheckedOffset = 0x1E4;         // CButtonWnd::Checked
constexpr int kDShowOffset = 0x196;           // CXWnd::dShow (visible flag)

// CListWnd (proven set from rcp_options_ui.cpp + the eqlib-mapped extras this
// window needs: per-cell icons, per-row data, per-cell colors, sort state).
const int kListAddString = ::Rcp::eqva(0x8580C0);   // int AddString(const CXStr&, COLORREF, uint32 data, void* pTa, char* tip)
const int kListGetCurSel = ::Rcp::eqva(0x853430);   // int GetCurSel() const
const int kListSetCurSel = ::Rcp::eqva(0x853470);   // void SetCurSel(int)
const int kListSetItemText = ::Rcp::eqva(0x8575B0); // void SetItemText(int row, int col, const CXStr&)
const int kListRemoveLine = ::Rcp::eqva(0x8582A0);  // void RemoveLine(int)
const int kListSetItemIcon = ::Rcp::eqva(0x857540); // void SetItemIcon(int row, int col, const CTextureAnimation*)
const int kListGetItemData = ::Rcp::eqva(0x853680); // uint32 GetItemData(int row) const
const int kListSetItemColor = ::Rcp::eqva(0x857770);   // void SetItemColor(int row, int col, COLORREF)
const int kListEnsureVisible = ::Rcp::eqva(0x8550D0);  // void EnsureVisible(int row)
const int kListSetColumnsSizable = ::Rcp::eqva(0x855F20);  // void SetColumnsSizable(bool) - enables header drag-resize
constexpr int kListItemsCountOffset = 0x1d8;  // ItemsArray.m_length == row count
constexpr int kListCurColOffset = 0x1fc;      // CurCol: column of the last click (next to proven CurSel@0x1f8)
constexpr int kListSortColOffset = 0x210;     // SortCol
constexpr int kListSortAscOffset = 0x214;     // bSortAsc (bool)
constexpr int kListFixedHeightOffset = 0x215; // bFixedHeight (bool)
constexpr int kListLineHeightOffset = 0x21c;  // LineHeight
constexpr int kListStyleOffset = 0x274;       // ListWndStyle
// Disasm of the DEFAULT CListWnd::OnHeaderClick (vtable+0x164 -> 0x8554b0): a header
// click only updates SortCol/bSortAsc + runs the native Sort when BOTH of these
// style bits are set at [this+0x274] - runtime-created lists lack them, which is
// why headers were display-only. OR them in at bind and native sorting just works.
constexpr uint32_t kListStyleSortable = 0x40000u | 0x400000u;

// CEditWnd: this build keeps the live typed text CXStr at +0x1a8 (disasm of
// CEditWnd::GetDisplayString@0x87B3E0: reads + refcounts [this+0x1a8]). The
// eqlib CEditBaseWnd::InputText@+0x1ec is a later-client layout - do not use.
constexpr int kEditInputTextOffset = 0x1a8;

// CXStr rep layout (same constants spell_icons.cpp / chat_stml_select.cpp use).
constexpr int kRepLength = 0x08, kRepEncoding = 0x0c, kRepData = 0x14;

// CTextureAnimation: vtable@0, Name CXStr@0x04, ..., CurCell@0x38, CellRect@0x3c..0x4b.
// Copies are raw malloc+memcpy of the SIDL manager's A_SpellIcons animation - the
// shared Frames array is only ever read, and SetCurCell touches per-copy fields.
constexpr int kTexAnimSize = 0x4c;
constexpr int kTexAnimNameOffset = 0x04;
const int kTexAnimSetCurCell = ::Rcp::eqva(0x87A860);  // void SetCurCell(int)

// Spell manager + EQ_Spell field offsets (eqlib game/Spells.h, 2013-05-10).
char **const kSpellMgr = reinterpret_cast<char **>(::Rcp::eqva(0xE646B0));  // ClientSpellManager**
constexpr int kSpellTableOffset = 0x2c180;
constexpr int kTotalSpellCount = 45001;
constexpr int kSpCastTime = 0x010;      // uint32 ms
constexpr int kSpRecastTime = 0x018;    // uint32 ms
constexpr int kSpDurationType = 0x01c;  // duration formula id
constexpr int kSpDurationCap = 0x020;   // duration cap (ticks)
constexpr int kSpManaCost = 0x028;      // int
constexpr int kSpBase = 0x02c;          // int[12] effect base values (description #N tokens)
constexpr int kSpMax = 0x08c;           // int[12] effect max values (description $N tokens)
constexpr int kSpDescriptionIdx = 0x154;
constexpr int kSpSpellIcon = 0x168;     // cell index into A_SpellIcons
constexpr int kSpCategory = 0x17c;
constexpr int kSpSubcategory = 0x180;
constexpr int kSpEnduranceCost = 0x194;
constexpr int kSpellCatDisciplines = 27;  // eEQSPELLCAT: SPELLCAT_DISCIPLINES
constexpr int kSpClassLevel = 0x246;    // uint8[36], index by class id (Warrior=1..)
constexpr int kSpTargetType = 0x26e;    // uint8 (eSpellTargetType)
constexpr int kSpSkill = 0x270;         // uint8 casting skill (EQEmu skills.h ids)
constexpr int kSpName = 0x27a;          // char[64] inline
constexpr int kSpScribable = 0x4e1;     // bool

// Character profile (eqlib PcClient.h/PcProfile.h).
char **const kLocalPC = reinterpret_cast<char **>(::Rcp::eqva(0xDD261C));
constexpr int kProfileMgrOffset = 0x31F0;  // ProfileManager {ProfileList* pFirst; int CurProfileList}
constexpr int kProfSpellBook = 0x2520;     // int[720]
constexpr int kProfMemorized = 0x3060;     // int[16] (12 usable gems)
constexpr int kProfClass = 0x3374;         // int
constexpr int kProfLevel = 0x3388;         // int
constexpr int kBookSlots = 720;
constexpr int kNumGems = 12;
// CharacterBase::standstate (uint8, pLocalPC-relative; same eqlib header region
// as the proven ProfileManager@+0x31F0). Spawn-appearance codes: 100 = standing,
// 110 = sitting.
constexpr int kStandStateOffset = 0x32f0;
constexpr uint8_t kStandStateStanding = 100;
constexpr uint8_t kStandStateSitting = 110;

// DB string table (category names + spell descriptions).
void **const kDbStr = reinterpret_cast<void **>(::Rcp::eqva(0xD1F380));  // pinstCDBStr
const int kDbGetString = ::Rcp::eqva(0x4866C0);  // const char* GetString(this, int id, int type, bool* found)
constexpr int kDbTypeSpellCategory = 5;
constexpr int kDbTypeSpellDescription = 6;

// CStmlWnd::SetSTMLText@0x883E10 (ret 0xC = 3 args: CXStr by value, bool
// addToHistory, SLinkInfo*). The generic vtable SetWindowText does NOT feed the
// STML parser on this build (pane stayed empty) - call this directly.
const int kStmlSetText = ::Rcp::eqva(0x883E10);

// Stock-window registry: CDisplay *(0xDD2660) + 0x2d84 = ScreenWndManager
// {ArrayClass<ScreenRecord> screens}: {int len; ScreenRecord* arr}, record
// stride 0x10, record+0 = CSidlScreenWnd** pWnd; window name = SidlText @+0x1dc.
// (Used only by the /rcpbook findwnd diagnostic - the windows the feature needs
// have direct instance globals below, found via disasm of the client's own
// memorize path: MemorizeSet@0x75CAC0 and the gem-drop handler @0x648905.)
char **const kDisplay = reinterpret_cast<char **>(::Rcp::eqva(0xDD2660));
constexpr int kGameScreensOffset = 0x2d84;
constexpr int kScreenRecSize = 0x10;
constexpr int kSidlTextOffset = 0x1dc;

// Stock window instances (disasm-verified globals for THIS build).
void **const kSpellBookWnd = reinterpret_cast<void **>(::Rcp::eqva(0xD1FC88));   // pinstCSpellBookWnd
void **const kCursorAttach = reinterpret_cast<void **>(::Rcp::eqva(0xD1FC7C));   // pinstCCursorAttachment

// CSpellBookWnd fields, disasm-verified (eqlib's MemTicksLeft@0x23c/ScribeTicks@
// 0x244 are NOT this build's layout - reading them as counters is what falsely
// reported "already memorizing"). StartSpellMemorization@0x75BF30 gates on BOTH
// of these being -1, so that pair IS the client's own busy check:
constexpr int kBookMemSlotOffset = 0x234;     // active memorize book slot, -1 = idle
constexpr int kBookScribeSlotOffset = 0x240;  // active scribe book slot, -1 = idle
constexpr int kBookMemSpellIdOffset = 0x238;  // spell id being memorized
constexpr int kBookMemTicksOffset = 0x23c;    // memorize countdown, seeded 0x5c by StartSpellMemorization
constexpr int kBookMemTicksMax = 0x5c;
// The book only processes (and ticks a memorize) while its "active" flag at
// +0x1d4 is set - the byte 0x864140 returns and OnProcessFrame@0x75CD20 gates
// on. A raw vtable Show() does NOT set it; the client's own open/close path is
// the Activate/Deactivate virtual pair, which is what the mod must use.
constexpr int kWndActiveFlagOffset = 0x1d4;
constexpr int kActivateVtOffset = 0x90;    // CXWnd vtable: Activate()
constexpr int kDeactivateVtOffset = 0x94;  // CXWnd vtable: Deactivate()

// CCursorAttachment, disasm-verified: Type@+0x224 (the gem-drop handler at
// 0x648905 requires Type==1 = MemorizeSpell and passes Index@+0x228 - the spell
// BOOK slot - into CSpellBookWnd::StartSpellMemorization@0x75BF30). Attach via
// the client's 6-arg overload @0x662290 (overlay, background, type, index,
// name, iconId - every stock site passes overlay/background null; the cursor
// renders the spell icon itself from the book slot). Cursor-free check =
// IsOkToActivate@0x660F30(type), the exact gate stock memorize paths use.
// IMPORTANT (in-game log + disasm of AttachToCursor's type dispatch @0x66225c):
// type 1 lands in the DEFAULT branch, which exits WITHOUT attaching unless an
// Overlay CTextureAnimation is provided - so the pickup must pass the spell's
// icon animation as the overlay (which is also what draws on the cursor).
constexpr int kCursorTypeOffset = 0x224;
const int kAttachToCursor6 = ::Rcp::eqva(0x662290);
const int kIsOkToActivate = ::Rcp::eqva(0x660F30);
constexpr int kCursorAttachMemorizeSpell = 1;

// ---- right-click-to-scribe (/rcpscribe), disasm-verified for THIS build ----
// The scribe engine mirrors the memorize one: BeginSpellScribe seeds book fields
// (+0x240 = book slot, +0x238 = spell id, +0x244 = 0x5c countdown) and the
// EQType-10 gauge provider @0x75E620 ticks them while ANY such gauge draws (our
// Rcp_SbScribeGauge or the stock book's); at zero it sends the request packet.
//
// void __thiscall CSpellBookWnd::BeginSpellScribe(int bookSlot) - ret 0x4. The
// stock book calls it when a slot is clicked with an item on the cursor (cursor
// attachment Type == 2). It validates EVERYTHING itself with native messages:
// no pending request, mem+scribe idle, cursor item is an unstacked type-20
// scroll, spell not known, class/level/membership ok, target slot empty (or
// same SpellGroup -> the native replace-lower-rank confirm dialog), then prints
// "You begin scribing %1." and seeds the countdown.
const int kBookBeginScribe = ::Rcp::eqva(0x75DDF0);
// void __thiscall CSpellBookWnd hide-worker @0x75B7C0 (no args) - what the
// stock book runs on EVERY close (Show(false) -> AboutToHide vtable+0xE0 ->
// this): detaches a type-1 cursor spell, prints the native "abandon" message
// for a pending memorize (str 0x2f0d) or scribe (str 0x2f0c), and resets all
// mem/scribe/set-mem state. Called on OUR window's hide edge so closing the
// new book (or standing up, which closes it) cancels exactly like stock.
const int kBookHideCancel = ::Rcp::eqva(0x75B7C0);
// void __thiscall CSpellBookWnd::TurnToPage(int leftPage) - ret 0x4; clamps to
// [0, 0x58], gated on the window's active flag (only used on the stock-book
// path to show the page being scribed).
const int kBookTurnToPage = ::Rcp::eqva(0x75D570);
// Global pending-request counter (both tickers inc it when they send; the
// begin functions refuse while it is > 0).
int *const kBookRequestsInFlight = reinterpret_cast<int *>(::Rcp::eqva(0xE635AC));
// ItemClient virtual-free helpers (thiscall on the ITEM; each resolves the
// definition via vtbl+0x8 internally):
const int kItemGetType = ::Rcp::eqva(0x7AFFA0);     // def byte @+0x1d4; 20 = spell scroll
const int kItemGetSpellId = ::Rcp::eqva(0x7AFFF0);  // (int spellType) ret 0x4; def dword @0x284 + type*0x64
constexpr int kItemTypeScroll = 20;
constexpr int kItemSpellTypeScroll = 4;    // eqlib ItemSpellType_Scroll
constexpr int kItemStackCountOffset = 0x98;  // ItemBase stack count (begin-scribe refuses > 1)
constexpr int kOffItemRefCount = 0x04;       // VeBaseReferenceCount::ReferenceCount
// bool __thiscall <pcSpellBookInfo>::IsSpellKnown(int spellId) - ret 0x4; this =
// pLocalPC + 0x2dc8; exact-compares all 720 profile book slots.
const int kSpellBookKnows = ::Rcp::eqva(0x449CD0);
constexpr int kSpellBookInfoOffset = 0x2dc8;
// ItemPtr* __thiscall <container>::GetItem(ItemPtr* out, int slot) - ret 0x8;
// this = pLocalPC + 8 + *(*(pLocalPC+8) + 4) (the vbase chain every stock call
// site uses); ADDREFS the item, so the extra reference is dropped immediately
// (the inventory keeps its own - same pattern as equip_item.cpp).
const int kContainerGetItem = ::Rcp::eqva(0x42DEC0);
constexpr int kInvSlotCursor = 33;  // 0x21 - the inventory cursor slot
// Spell fields for the rank pre-check (eqlib Spells.h, matches the disasm).
constexpr int kSpSpellGroup = 0x1d4;  // int: rank family (0 = ungrouped)
constexpr int kSpSpellRank = 0x1dc;   // int: rank within the group
constexpr int kSpIsSkill = 0x244;     // bool: BeginSpellScribe silently refuses these
// CInvSlotMgr::MoveItem (proven in equip_item.cpp) - moves the scroll onto the
// inventory cursor, which is where both BeginSpellScribe and the server-side
// scribe completion expect it.
const int kInvSlotMgrMoveItem = ::Rcp::eqva(0x698D80);
void **const kInvSlotMgr = reinterpret_cast<void **>(::Rcp::eqva(0xD1FD80));  // pinstCInvSlotMgr
// Stack mirror of eqlib's ItemGlobalIndex (0x0c bytes).
struct ScribeGlobalIndex {
  int location;
  short slots[3];
  short pad;
};

// Splitter (drag the divider between list and description): game mouse position
// MQMouseInfo{int X; int Y} @0xE67B54 (eqlib __Mouse). Rect fields, settled by
// disasm + in-game rect dumps: the LIVE CXWnd::Location CXRect {l,t,r,b} is at
// **+0x60** (SetLocation@0x866AA0 - the worker behind the Move virtual - reads
// and updates it); +0xC0 is OldLocation scratch that only Resize@0x863990
// writes (children show ZEROS there; the window shows its stale XML design
// rect). Resize itself takes FIVE args (w,h,1,0,0; ret 0x14) and preserves the
// top-left, so all layout goes through the Move virtual instead.
const uintptr_t kMouseInfo = ::Rcp::eqva(0xE67B54);
constexpr int kWndLocationOffset = 0x60;
const int kResize = ::Rcp::eqva(0x863990);
constexpr int kSplitGap = 8;      // divider drag-handle height (matches the generator layout)
constexpr int kListMinCY = 96;    // never shrink the list below ~3 rows + header
constexpr int kDescMinCY = 40;    // never shrink the description below this
// Fallback frame paddings (right/bottom gap between the child edges and the
// window frame) if the create-time capture looks insane. The real values are
// CAPTURED from the freshly created window (see g_d_right/g_d_bottom): the
// XML-design constants cannot be used directly because the +0xC0 Location rect
// includes non-client decoration (titlebar/borders) of unknown size.
constexpr int kLayPadFallback = 8;
// Column-width access for persistence (SListWndColumn stride 0x3c, Width@+0).
constexpr int kListColumnsOffset = 0x1e8;  // ArrayClass<SListWndColumn> {len@0, arr@4}
constexpr int kColRecSize = 0x3c;
const int kListSetColumnWidth = ::Rcp::eqva(0x853BF0);

// ---- settings ----
bool g_enabled = false;         // "[SpellBook] NewWindow": replace the stock book
bool g_show_unscribed = false;  // "[SpellBook] ShowUnscribed"
bool g_scribe_enabled = false;  // "[SpellBook] RightClickScribe": /rcpscribe
// Persisted geometry ([SpellBook] ini): window pos+size, split, column widths.
// Key prefix versioned with the column layout so stale widths never misapply
// to shifted columns after a column is added.
constexpr int kGeoCols = 11;  // == kColCount (enum defined below; static_assert there)
constexpr char kGeoColKey[] = "ColXW%d";
int g_geo_winx = -100000, g_geo_winy = -100000;
int g_geo_winw = 0, g_geo_winh = 0;
int g_geo_listcy = 0;
int g_geo_colw[kGeoCols] = {};

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKeyEnabled)) g_enabled = ini.getValue<bool>(kIniSection, kIniKeyEnabled);
  if (ini.exists(kIniSection, kIniKeyUnscribed)) g_show_unscribed = ini.getValue<bool>(kIniSection, kIniKeyUnscribed);
  if (ini.exists(kIniSection, kIniKeyScribe)) g_scribe_enabled = ini.getValue<bool>(kIniSection, kIniKeyScribe);
  if (ini.exists(kIniSection, "WinX")) g_geo_winx = ini.getValue<int>(kIniSection, "WinX");
  if (ini.exists(kIniSection, "WinY")) g_geo_winy = ini.getValue<int>(kIniSection, "WinY");
  if (ini.exists(kIniSection, "WinW")) g_geo_winw = ini.getValue<int>(kIniSection, "WinW");
  if (ini.exists(kIniSection, "WinH")) g_geo_winh = ini.getValue<int>(kIniSection, "WinH");
  if (ini.exists(kIniSection, "ListCY")) g_geo_listcy = ini.getValue<int>(kIniSection, "ListCY");
  for (int i = 0; i < kGeoCols; ++i) {
    char key[16];
    std::snprintf(key, sizeof(key), kGeoColKey, i);
    if (ini.exists(kIniSection, key)) g_geo_colw[i] = ini.getValue<int>(kIniSection, key);
  }
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, kIniKeyEnabled, g_enabled);
  ini.setValue<bool>(kIniSection, kIniKeyUnscribed, g_show_unscribed);
  ini.setValue<bool>(kIniSection, kIniKeyScribe, g_scribe_enabled);
}

void save_geometry() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<int>(kIniSection, "WinX", g_geo_winx);
  ini.setValue<int>(kIniSection, "WinY", g_geo_winy);
  ini.setValue<int>(kIniSection, "WinW", g_geo_winw);
  ini.setValue<int>(kIniSection, "WinH", g_geo_winh);
  ini.setValue<int>(kIniSection, "ListCY", g_geo_listcy);
  for (int i = 0; i < kGeoCols; ++i) {
    char key[16];
    std::snprintf(key, sizeof(key), kGeoColKey, i);
    ini.setValue<int>(kIniSection, key, g_geo_colw[i]);
  }
}

// ---- window / model state (file-static; dropped wholesale by drop_handles) ----
void *g_wnd = nullptr;
void *g_list = nullptr;
void *g_edit = nullptr;
void *g_cb_unscribed = nullptr;
void *g_lbl_status = nullptr;
void *g_stml = nullptr;
void *g_btn_split = nullptr;     // the divider drag-handle button
void *g_combo_filter = nullptr;  // the skill/category/subcategory filter picker
bool g_create_attempted = false;

bool g_mem_was_open = false;  // the stock book was open for a memorize animation

struct SpellRow {
  int spell_id;
  bool scribed;
};
std::vector<SpellRow> g_rows;      // current list order (row index -> spell)
std::vector<int> g_class_spells;   // all spell ids usable by g_class_cache (unscribed source)
int g_class_cache = -1;            // class the cache was built for
uint32_t g_last_sig = 0;           // rebuild signature (0 = force rebuild)
std::string g_search;              // current lowercased search text
int g_sel_spell = -1;              // spell shown in the description pane
int g_last_sel = -1;               // last GetCurSel seen (click edge detection)
int g_last_col = -1;               // last CurCol seen (so a gem click on the SELECTED row still fires)
// Filter picker state: what the selected choice means, and the choice list
// (index-parallel with g_filter_map: {kind, value}; kind 0=all, 1=skill,
// 2=category, 3=subcategory).
int g_filter_kind = 0;
int g_filter_value = 0;
std::vector<std::string> g_filter_choices;
std::vector<std::pair<int, int>> g_filter_map;
int g_last_filter_choice = -1;
bool g_last_unscribed_cb = false;
// OUR sort state. The native OnHeaderClick is used only as a header-CLICK
// DETECTOR: it can fire more than once per physical click (its bSortAsc toggle
// nets to zero on a same-column click, which is how "sorting is stuck one way"
// happened), so the direction decision is debounced here and the native fields
// are re-normalized after every observed change.
int g_sort_col = 2;       // == kColLvl (column enum is defined below); default = level ascending
bool g_sort_asc = true;
int g_sort_cooldown = 0;  // frames left in which further native sort-field changes are the same click
// Splitter drag state: started by the native press on the Rcp_SbSplit handle
// button, then tracked by RELATIVE mouse movement (no absolute coordinate-space
// assumptions between CXWnd rects and the mouse globals).
bool g_split_drag = false;
int g_split_start_my = 0;      // game-mouse Y at drag start
int g_split_start_list_h = 0;  // list height at drag start
// Child anchoring. The +0xC0 Location rects of CHILDREN are only populated
// after the window's first draw - anything read at create time is zeros. So all
// geometry work waits until the rects turn valid (g_anchored), then children
// are anchored to the WINDOW rect via the captured margins.
bool g_anchored = false;
int g_d_left = kLayPadFallback;    // list.left - window.left
int g_d_ltop = 28;                 // list.top - window.top
int g_d_right = kLayPadFallback;   // window.right - list.right
int g_d_bottom = kLayPadFallback;  // window.bottom - desc.bottom
int g_lay_l = 0, g_lay_t = 0, g_lay_w = 0, g_lay_h = 0;  // last laid-out window rect (w=0 = force)
int g_open_grace = 0;  // frames since open during which the stand-check is suspended
int g_geo_poll_cd = 0;         // frames until the next geometry-persistence check
int g_pending_listcy = 0;      // saved split to apply after the first relayout
// Deferred /sit: toggle_window can run INSIDE the InterpretCmd dispatch (typed
// /rcpbook, hotbutton/social) and the command interpreter is NOT reentrant -
// issuing "/sit on" from there corrupted the outer caller's stack frame (the
// post-dispatch SetWindowText call went through a trashed window local = the
// jump-to-stack crash). The sit is flagged here and issued from the next
// ProcessGameEvents frame instead.
bool g_want_sit = false;
bool g_mem_label = false;  // the status label currently shows the memorize bar
int g_mem_log_cd = 0;      // throttle for the mem-tick debug log
// Right-click-to-scribe pending request: the detour moved the scroll onto the
// cursor and queued the rest (open the book + BeginSpellScribe) for the frame
// poll, so no windows are opened from inside the inventory click handler.
int g_pending_scribe_spell = 0;   // 0 = none
int g_pending_scribe_frames = 0;  // retry budget waiting for the cursor move
// Our window's visibility last frame - the hide edge cancels a pending
// memorize/scribe via the stock book's own hide worker (stock-close parity).
bool g_book_was_visible = false;

// ---- thin __thiscall wrappers (rcp_options_ui.cpp pattern) ----
void cxstr_init(void *buf, const char *s) {
  *reinterpret_cast<uint32_t *>(buf) = 0;
  reinterpret_cast<void(__thiscall *)(void *, const char *)>(kCXStrCtor)(buf, s);
}

void *get_child(void *wnd, const char *name) {
  if (!wnd) return nullptr;
  uint32_t name_cxstr;
  cxstr_init(&name_cxstr, name);
  return reinterpret_cast<void *(__thiscall *)(void *, void *, int)>(kGetChildItem)(wnd, &name_cxstr, 0);
}

bool checkbox_get(void *cb) {
  return cb ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(cb) + kCheckedOffset) != 0 : false;
}
void checkbox_set(void *cb, bool checked) {
  if (cb) *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(cb) + kCheckedOffset) = checked ? 1 : 0;
}
bool is_visible(void *wnd) {
  return wnd ? *reinterpret_cast<uint8_t *>(reinterpret_cast<char *>(wnd) + kDShowOffset) != 0 : false;
}
void wnd_call_void(void *wnd, int vt_offset) {
  if (!wnd) return;
  void *vtable = *reinterpret_cast<void **>(wnd);
  void *fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + vt_offset);
  reinterpret_cast<void(__thiscall *)(void *)>(fn)(wnd);
}
void show_window(void *wnd, bool show) {
  if (!wnd) return;
  void *vtable = *reinterpret_cast<void **>(wnd);
  void *show_fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + kShowVtOffset);
  reinterpret_cast<int(__thiscall *)(void *, int, int, int)>(show_fn)(wnd, show ? 1 : 0, 1, 1);
}
// Virtual SetWindowText(const CXStr&) - for plain labels.
void set_window_text(void *wnd, const char *text) {
  if (!wnd) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  void *vtable = *reinterpret_cast<void **>(wnd);
  void *fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + kSetWindowTextVtOffset);
  reinterpret_cast<void(__thiscall *)(void *, const void *)>(fn)(wnd, &cxstr);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);
}

// CStmlWnd::SetSTMLText: the CXStr is passed BY VALUE, so the callee consumes
// our reference (MSVC destroys by-value class params in the callee) - do NOT
// run the CXStr destructor here, that would double-release the rep.
void stml_set_text(void *stml, const char *text) {
  if (!stml) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  reinterpret_cast<void(__thiscall *)(void *, uint32_t, int, void *)>(kStmlSetText)(stml, cxstr, 1, nullptr);
}

int list_row_count(void *list) {
  return list ? *reinterpret_cast<int *>(reinterpret_cast<char *>(list) + kListItemsCountOffset) : 0;
}
void list_clear(void *list) {
  if (!list) return;
  for (int i = list_row_count(list) - 1; i >= 0; --i)
    reinterpret_cast<void(__thiscall *)(void *, int)>(kListRemoveLine)(list, i);
}
// Adds a row: col-0 text (empty here - col 0 is the icon cell), per-row COLORREF,
// the row Data (we store the spell id), and the col-0 CTextureAnimation icon.
int list_add_row(void *list, const char *text, uint32_t argb, uint32_t data, void *ta) {
  if (!list) return -1;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  int idx = reinterpret_cast<int(__thiscall *)(void *, const void *, uint32_t, uint32_t, void *, const char *)>(
      kListAddString)(list, &cxstr, argb, data, ta, nullptr);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);
  return idx;
}
void list_set_item_text(void *list, int row, int col, const char *text) {
  if (!list) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  reinterpret_cast<void(__thiscall *)(void *, int, int, const void *)>(kListSetItemText)(list, row, col, &cxstr);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);
}
int list_get_cur_sel(void *list) {
  return list ? reinterpret_cast<int(__thiscall *)(const void *)>(kListGetCurSel)(list) : -1;
}
void list_set_cur_sel(void *list, int row) {
  if (list) reinterpret_cast<void(__thiscall *)(void *, int)>(kListSetCurSel)(list, row);
}
uint32_t list_get_item_data(void *list, int row) {
  return list ? reinterpret_cast<uint32_t(__thiscall *)(const void *, int)>(kListGetItemData)(list, row) : 0;
}
void list_set_item_color(void *list, int row, int col, uint32_t argb) {
  if (list) reinterpret_cast<void(__thiscall *)(void *, int, int, uint32_t)>(kListSetItemColor)(list, row, col, argb);
}
void list_ensure_visible(void *list, int row) {
  if (list) reinterpret_cast<void(__thiscall *)(void *, int)>(kListEnsureVisible)(list, row);
}
// ---- CComboWnd wrappers (the filter picker; proven offsets from the options
// window). Every method derefs pListWnd@+0x1d8 with no null check, so all
// wrappers gate on it.
const int kComboDeleteAll = ::Rcp::eqva(0x86A960);
const int kComboInsertChoice = ::Rcp::eqva(0x86AE50);
const int kComboGetCurChoice = ::Rcp::eqva(0x86A780);
const int kComboSetChoice = ::Rcp::eqva(0x86A740);
constexpr int kComboListWndOffset = 0x1d8;
bool combo_ready(void *combo) {
  return combo && *reinterpret_cast<void **>(reinterpret_cast<char *>(combo) + kComboListWndOffset);
}
void combo_delete_all(void *combo) {
  if (combo_ready(combo)) reinterpret_cast<void(__thiscall *)(void *)>(kComboDeleteAll)(combo);
}
void combo_insert_choice(void *combo, const char *text) {
  if (!combo_ready(combo)) return;
  uint32_t cxstr;
  cxstr_init(&cxstr, text);
  reinterpret_cast<int(__thiscall *)(void *, const void *, uint32_t)>(kComboInsertChoice)(combo, &cxstr, 0);
  reinterpret_cast<void(__thiscall *)(void *)>(kCXStrDtor)(&cxstr);
}
int combo_get_cur_choice(void *combo) {
  return combo_ready(combo) ? reinterpret_cast<int(__thiscall *)(const void *)>(kComboGetCurChoice)(combo) : -1;
}
void combo_set_choice(void *combo, int index) {
  if (combo_ready(combo)) reinterpret_cast<void(__thiscall *)(void *, int)>(kComboSetChoice)(combo, index);
}
bool combo_popup_open(void *combo) {
  if (!combo) return false;
  void *list = *reinterpret_cast<void **>(reinterpret_cast<char *>(combo) + kComboListWndOffset);
  return list && is_visible(list);
}

int list_get_col_width(void *list, int col) {
  if (!list) return 0;
  char *lw = reinterpret_cast<char *>(list);
  const int len = *reinterpret_cast<int *>(lw + kListColumnsOffset);
  char *arr = *reinterpret_cast<char **>(lw + kListColumnsOffset + 4);
  if (!arr || col < 0 || col >= len) return 0;
  return *reinterpret_cast<int *>(arr + col * kColRecSize);
}
void list_set_col_width(void *list, int col, int w) {
  if (list && w > 0) reinterpret_cast<void(__thiscall *)(void *, int, int)>(kListSetColumnWidth)(list, col, w);
}

// POD-safe (rcp_guard-friendly) CXStr equality against a C string.
bool cxstr_eq(const void *cxstr_field, const char *want) {
  char *rep = *reinterpret_cast<char *const *>(cxstr_field);
  if (!rep || reinterpret_cast<uintptr_t>(rep) < 0x10000) return false;
  const uint32_t len = *reinterpret_cast<uint32_t *>(rep + kRepLength);
  const uint32_t enc = *reinterpret_cast<uint32_t *>(rep + kRepEncoding);
  if (enc != 0 || len == 0 || len > 256) return false;
  const size_t want_len = std::strlen(want);
  if (len != want_len) return false;
  return _strnicmp(rep + kRepData, want, want_len) == 0;
}

// Reads the search box's live typed text (the CXStr @+0x1a8, per the
// GetDisplayString disasm). Handles BOTH rep encodings - typed edit-box text is
// commonly a UTF-16 rep (encoding 1), which the previous ASCII-only read
// silently discarded (= "search does nothing"). Guarded + POD body so a layout
// surprise degrades to an empty read instead of a crash.
std::string edit_get_text(void *edit) {
  char buf[256];
  int n = 0;
  if (edit) {
    rcp_guard::run("spellbook.edit", [&]() {
      char *rep = *reinterpret_cast<char **>(reinterpret_cast<char *>(edit) + kEditInputTextOffset);
      if (!rep || reinterpret_cast<uintptr_t>(rep) < 0x10000) return;
      const uint32_t len = *reinterpret_cast<uint32_t *>(rep + kRepLength);
      const uint32_t enc = *reinterpret_cast<uint32_t *>(rep + kRepEncoding);
      if (len > 255) return;
      if (enc == 0) {  // 8-bit chars
        n = static_cast<int>(len);
        std::memcpy(buf, rep + kRepData, len);
      } else if (enc == 1) {  // UTF-16: narrow the low bytes (search is ASCII anyway)
        const uint16_t *w = reinterpret_cast<const uint16_t *>(rep + kRepData);
        for (uint32_t i = 0; i < len; ++i) buf[i] = static_cast<char>(w[i] < 0x80 ? w[i] : '?');
        n = static_cast<int>(len);
      }
    });
  }
  return std::string(buf, buf + n);
}

std::string to_lower(std::string s) {
  for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// ---- game-data accessors ----
char *spell_by_id(int id) {
  char *mgr = *kSpellMgr;
  if (!mgr || id <= 0 || id >= kTotalSpellCount) return nullptr;
  return *reinterpret_cast<char **>(mgr + kSpellTableOffset + id * 4);
}

uint8_t player_standstate() {
  char *pc = *kLocalPC;
  return pc ? *reinterpret_cast<uint8_t *>(pc + kStandStateOffset) : 0;
}

// Sit down ONLY when not already sitting: the stock /sit handler toggles, so an
// unconditional "/sit on" while seated STANDS the player (which is what was
// cancelling memorizes started from a seated position).
void sit_if_needed() {
  const uint8_t ss = player_standstate();
  logger::logf("[book] sit_if_needed: standstate=%d", ss);
  if (ss != kStandStateSitting) ForwardCommand("/sit on");
}

// Walks pLocalPC's ProfileManager to the current PcProfile (guarded POD walk).
char *get_pc_profile() {
  char *result = nullptr;
  rcp_guard::run("spellbook.profile", [&]() {
    char *pc = *kLocalPC;
    if (!pc) return;
    char *list = *reinterpret_cast<char **>(pc + kProfileMgrOffset);
    const int cur = *reinterpret_cast<int *>(pc + kProfileMgrOffset + 4);
    for (int hop = 0; list && hop < 8; ++hop) {
      if (*reinterpret_cast<int *>(list) == cur) {
        result = *reinterpret_cast<char **>(list + 4);  // ProfileList::pFirst
        return;
      }
      list = *reinterpret_cast<char **>(list + 0x0c);  // ProfileList::pNext
    }
  });
  return result;
}

std::string db_string(int id, int type) {
  void *dbstr = *kDbStr;
  if (!dbstr || id <= 0) return std::string();
  const char *s = reinterpret_cast<const char *(__thiscall *)(void *, int, int, void *)>(kDbGetString)(
      dbstr, id, type, nullptr);
  return s ? std::string(s) : std::string();
}

// Finds a registered game screen window by SIDL name (guarded POD walk of
// CDisplay's gameScreens registry - every entry is a CSidlScreenWnd).
void *find_game_screen(const char *name) {
  void *found = nullptr;
  rcp_guard::run("spellbook.findwnd", [&]() {
    char *disp = *kDisplay;
    if (!disp) return;
    char *mgr = disp + kGameScreensOffset;
    const int len = *reinterpret_cast<int *>(mgr);
    char *arr = *reinterpret_cast<char **>(mgr + 4);
    if (!arr || len <= 0 || len > 4096) return;
    for (int i = 0; i < len; ++i) {
      char **ppwnd = *reinterpret_cast<char ***>(arr + i * kScreenRecSize);
      if (!ppwnd || !*ppwnd) continue;
      if (cxstr_eq(*ppwnd + kSidlTextOffset, name)) {
        found = *ppwnd;
        return;
      }
    }
  });
  return found;
}

// ---- spell-icon cell copies ----
// SetCurCell is stateful on the animation object, so every distinct icon needs
// its own CTextureAnimation. Copies are malloc+memcpy of the manager's
// A_SpellIcons (Frames array shared read-only, Name zeroed so no CXStr refcount
// aliasing), SetCurCell'd once, cached per cell index, and plain-free'd on drop
// (never destructed - nothing owned).
std::map<int, void *> g_icon_copies;
char *g_icons_anim = nullptr;

void *icon_for_cell(int cell) {
  auto it = g_icon_copies.find(cell);
  if (it != g_icon_copies.end()) return it->second;
  if (!g_icons_anim) g_icons_anim = spell_icons_api::find_animation("A_SpellIcons");
  if (!g_icons_anim) return nullptr;
  void *copy = std::malloc(kTexAnimSize);
  if (!copy) return nullptr;
  std::memcpy(copy, g_icons_anim, kTexAnimSize);
  *reinterpret_cast<void **>(reinterpret_cast<char *>(copy) + kTexAnimNameOffset) = nullptr;
  reinterpret_cast<void(__thiscall *)(void *, int)>(kTexAnimSetCurCell)(copy, cell);
  g_icon_copies[cell] = copy;
  return copy;
}

void free_icon_copies() {
  for (auto &kv : g_icon_copies) std::free(kv.second);
  g_icon_copies.clear();
  g_icons_anim = nullptr;
}

// ---- column model ----
// Order/widths are the contract with tools/gen_rcp_spellbook_ui.py (COLUMNS).
enum { kColGem = 0, kColName, kColLvl, kColMana, kColCast, kColRecast, kColDur, kColTarget, kColSkill,
       kColCat, kColSub, kColCount };
static_assert(kGeoCols == kColCount, "geometry persistence must cover every column");

// EQEmu's CalcBuffDuration_formula (akk-stack code/zone/spells.cpp) - the same
// table the client uses. Returns ticks; negative = permanent.
int duration_ticks(int level, int formula, int cap) {
  int t;
  switch (formula) {
    case 0: return 0;
    case 1: t = level > 3 ? level / 2 : 1; break;
    case 2: t = level > 3 ? level / 2 + 5 : 6; break;
    case 3: t = 30 * level; break;
    case 4: t = 50; break;
    case 5: t = 2; break;
    case 6: t = level / 2 + 2; break;
    case 7: t = level; break;
    case 8: t = level + 10; break;
    case 9: t = 2 * level + 10; break;
    case 10: t = 3 * level + 10; break;
    case 11: t = 30 * (level + 3); break;
    case 12: t = level > 7 ? level / 4 : 1; break;
    case 13: t = 4 * level + 10; break;
    case 14: t = 5 * (level + 2); break;
    case 15: t = 10 * (level + 10); break;
    case 50: return -1;  // permanent
    case 51: return -4;  // permanent (aura)
    default:
      if (formula < 200) return 0;
      t = formula;
      break;
  }
  if (cap && cap < t) t = cap;
  return t;
}

std::string fmt_cast(uint32_t ms) {
  if (ms == 0) return "-";
  char b[24];
  std::snprintf(b, sizeof(b), "%.1fs", ms / 1000.0);
  return b;
}

std::string fmt_recast(uint32_t ms) {
  if (ms == 0) return "-";
  char b[24];
  if (ms < 60000)
    std::snprintf(b, sizeof(b), "%.1fs", ms / 1000.0);
  else
    std::snprintf(b, sizeof(b), "%u:%02u", ms / 60000, (ms / 1000) % 60);
  return b;
}

std::string fmt_duration(int ticks) {
  if (ticks < 0) return "perm";
  if (ticks == 0) return "-";
  const int secs = ticks * 6;
  char b[24];
  if (secs < 60)
    std::snprintf(b, sizeof(b), "%ds", secs);
  else if (secs < 3600)
    std::snprintf(b, sizeof(b), "%d:%02d", secs / 60, secs % 60);
  else
    std::snprintf(b, sizeof(b), "%dh%02dm", secs / 3600, (secs % 3600) / 60);
  return b;
}

// Casting-skill names (EQEmu common/skills.h ids; the interesting subset).
const char *skill_name(uint8_t s) {
  switch (s) {
    case 4: return "Abjuration";
    case 5: return "Alteration";
    case 12: return "Brass";
    case 13: return "Channeling";
    case 14: return "Conjuration";
    case 18: return "Divination";
    case 24: return "Evocation";
    case 28: return "Hand to Hand";
    case 33: return "Offense";
    case 36: return "Piercing";
    case 41: return "Singing";
    case 49: return "Strings";
    case 51: return "Throwing";
    case 52: return "Tiger Claw";
    case 54: return "Winds";
    default: {
      static char b[16];
      std::snprintf(b, sizeof(b), "#%d", s);
      return b;
    }
  }
}

const char *target_name(uint8_t t) {
  switch (t) {
    case 1: return "LoS";
    case 2: case 40: return "AE PC";
    case 3: case 41: return "Group";
    case 4: return "PB AE";
    case 5: return "Single";
    case 6: return "Self";
    case 8: return "Targeted AE";
    case 9: return "Animal";
    case 10: return "Undead";
    case 11: return "Summoned";
    case 13: return "Lifetap";
    case 14: case 38: return "Pet";
    case 15: return "Corpse";
    case 16: return "Plant";
    case 17: return "Giant";
    case 18: return "Dragon";
    case 20: return "AE Lifetap";
    case 24: return "AE Undead";
    case 25: return "AE Summoned";
    case 32: case 33: return "Hate list";
    case 36: return "Area PC";
    case 37: return "Area NPC";
    case 39: return "Target PC";
    case 42: return "Cone";
    case 43: return "Group single";
    case 44: return "Beam";
    case 45: return "Free target";
    case 46: return "Target's target";
    case 47: return "Pet owner";
    case 50: return "Area enemies";
    case 52: return "Beneficial";
    default: {
      static char b[16];
      std::snprintf(b, sizeof(b), "#%d", t);
      return b;
    }
  }
}

int spell_class_level(char *sp, int cls) {
  if (cls < 0 || cls > 35) return 255;
  return *reinterpret_cast<uint8_t *>(sp + kSpClassLevel + cls);
}

std::string col_cell_text(int col, char *sp, int cls, int level) {
  char b[32];
  switch (col) {
    case kColGem:
      return std::string();
    case kColName:
      return std::string(sp + kSpName);
    case kColLvl: {
      const int lv = spell_class_level(sp, cls);
      if (lv >= 255) return "-";
      std::snprintf(b, sizeof(b), "%d", lv);
      return b;
    }
    case kColMana: {
      const int mana = *reinterpret_cast<int *>(sp + kSpManaCost);
      if (mana <= 0) return "-";
      std::snprintf(b, sizeof(b), "%d", mana);
      return b;
    }
    case kColCast:
      return fmt_cast(*reinterpret_cast<uint32_t *>(sp + kSpCastTime));
    case kColRecast:
      return fmt_recast(*reinterpret_cast<uint32_t *>(sp + kSpRecastTime));
    case kColDur:
      return fmt_duration(duration_ticks(level, *reinterpret_cast<int *>(sp + kSpDurationType),
                                         *reinterpret_cast<int *>(sp + kSpDurationCap)));
    case kColTarget:
      return target_name(*reinterpret_cast<uint8_t *>(sp + kSpTargetType));
    case kColSkill:
      return skill_name(*reinterpret_cast<uint8_t *>(sp + kSpSkill));
    case kColCat:
      return db_string(*reinterpret_cast<int *>(sp + kSpCategory), kDbTypeSpellCategory);
    case kColSub:
      return db_string(*reinterpret_cast<int *>(sp + kSpSubcategory), kDbTypeSpellCategory);
    default:
      return std::string();
  }
}

bool col_is_numeric(int col) {
  return col == kColLvl || col == kColMana || col == kColCast || col == kColRecast || col == kColDur;
}

long long col_num_key(int col, char *sp, int cls, int level) {
  switch (col) {
    case kColLvl: return spell_class_level(sp, cls);
    case kColMana: return *reinterpret_cast<int *>(sp + kSpManaCost);
    case kColCast: return *reinterpret_cast<uint32_t *>(sp + kSpCastTime);
    case kColRecast: return *reinterpret_cast<uint32_t *>(sp + kSpRecastTime);
    case kColDur: {
      const int t = duration_ticks(level, *reinterpret_cast<int *>(sp + kSpDurationType),
                                   *reinterpret_cast<int *>(sp + kSpDurationCap));
      return t < 0 ? 0x7FFFFFFFLL : t;  // permanent sorts as longest
    }
    default: return 0;
  }
}

// ---- model refresh ----
// True for the spell-file filler that isn't a real player spell (the 45k table
// is mostly NPC/test/placeholder entries; class level + scribable alone let a
// lot of it through). Category 0 = uncategorized (every real player spell the
// stock book can show carries a category), plus the common junk name shapes.
bool is_junk_spell(char *sp) {
  if (*reinterpret_cast<int *>(sp + kSpCategory) <= 0) return true;
  const char *name = sp + kSpName;
  if (!name[0]) return true;
  if (_strnicmp(name, "test", 4) == 0 || _strnicmp(name, "n/a", 3) == 0 || _strnicmp(name, "na ", 3) == 0 ||
      _strnicmp(name, "sku", 3) == 0 || _strnicmp(name, "reserved", 8) == 0)
    return true;
  return false;
}

// View filter (both the book and the unscribed list): hide disciplines /
// endurance-based combat abilities, anything with no usable level for the
// class, and anything without BOTH a category and a subcategory (real player
// spells always carry both; the rest is spell-file filler).
bool row_visible(char *sp, int cls) {
  const int lv = spell_class_level(sp, cls);
  if (lv < 1 || lv > 253) return false;
  const int cat = *reinterpret_cast<int *>(sp + kSpCategory);
  if (cat <= 0 || cat == kSpellCatDisciplines) return false;
  if (*reinterpret_cast<int *>(sp + kSpSubcategory) <= 0) return false;
  if (*reinterpret_cast<int *>(sp + kSpEnduranceCost) > 0) return false;
  return true;
}

// Rank handling for "Spell", "Spell Rk. II", "Spell Rk. III" families: returns
// the rank (1..3) and fills `base` with the lowercased family name (suffix
// stripped). Matches the ". II"/".II" spacing variants; III checked first.
int spell_rank_and_base(const char *name, std::string &base) {
  base = to_lower(name);
  auto strip = [&](const char *suf) {
    const size_t n = std::strlen(suf);
    if (base.size() > n && base.compare(base.size() - n, n, suf) == 0) {
      base.resize(base.size() - n);
      return true;
    }
    return false;
  };
  if (strip(" rk. iii") || strip(" rk.iii")) return 3;
  if (strip(" rk. ii") || strip(" rk.ii")) return 2;
  return 1;
}

__attribute__((noinline)) void ensure_class_spells(int cls) {
  if (g_class_cache == cls && !g_class_spells.empty()) return;
  g_class_spells.clear();
  g_class_spells.reserve(1024);
  g_class_cache = cls;
  char *mgr = *kSpellMgr;
  if (!mgr) return;
  for (int id = 1; id < kTotalSpellCount; ++id) {
    char *sp = *reinterpret_cast<char **>(mgr + kSpellTableOffset + id * 4);
    if (!sp) continue;
    const int lv = spell_class_level(sp, cls);
    if (lv < 1 || lv > 254) continue;  // 255 = class cannot use it (level cap applied per rebuild)
    if (!*reinterpret_cast<uint8_t *>(sp + kSpScribable)) continue;
    if (is_junk_spell(sp)) continue;
    g_class_spells.push_back(id);
  }
  logger::logf("[book] class-spell cache built: class=%d usable=%d", cls, (int)g_class_spells.size());
}

// Rebuild signature: any change to what determines row set/order/content forces a
// repaint. MemorizedSpells is deliberately NOT hashed - memorizing does not change
// any cell today, and skipping it keeps the scroll position through a memorize.
uint32_t fnv_step(uint32_t h, const void *data, size_t n) {
  const uint8_t *p = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

uint32_t compute_sig(char *prof, const std::string &search) {
  uint32_t h = 2166136261u;
  h = fnv_step(h, prof + kProfSpellBook, kBookSlots * 4);
  const int cls = *reinterpret_cast<int *>(prof + kProfClass);
  const int lvl = *reinterpret_cast<int *>(prof + kProfLevel);
  h = fnv_step(h, &cls, 4);
  h = fnv_step(h, &lvl, 4);
  const uint8_t un = g_show_unscribed ? 1 : 0;
  h = fnv_step(h, &un, 1);
  h = fnv_step(h, &g_sort_col, 4);
  const uint8_t asc = g_sort_asc ? 1 : 0;
  h = fnv_step(h, &asc, 1);
  h = fnv_step(h, &g_filter_kind, 4);
  h = fnv_step(h, &g_filter_value, 4);
  if (!search.empty()) h = fnv_step(h, search.data(), search.size());
  return h ? h : 1;  // 0 is the "force rebuild" sentinel
}

bool filter_matches(char *sp) {
  switch (g_filter_kind) {
    case 1: return *reinterpret_cast<uint8_t *>(sp + kSpSkill) == g_filter_value;
    case 2: return *reinterpret_cast<int *>(sp + kSpCategory) == g_filter_value;
    case 3: return *reinterpret_cast<int *>(sp + kSpSubcategory) == g_filter_value;
    default: return true;
  }
}

// Rebuild the filter-picker choices from the skills/categories/subcategories
// present in the current (unfiltered) row universe. Never rebuilds while the
// user is browsing the popup; keeps the active selection if still present.
void populate_filter_combo(std::vector<int> skills, std::vector<int> cats, std::vector<int> subs) {
  if (!g_combo_filter || combo_popup_open(g_combo_filter)) return;
  auto by_name = [](std::vector<int> &v, auto name_of) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    std::sort(v.begin(), v.end(), [&](int a, int b) { return name_of(a) < name_of(b); });
  };
  by_name(skills, [](int s) { return std::string(skill_name(static_cast<uint8_t>(s))); });
  by_name(cats, [](int c) { return db_string(c, kDbTypeSpellCategory); });
  by_name(subs, [](int c) { return db_string(c, kDbTypeSpellCategory); });
  std::vector<std::string> choices;
  std::vector<std::pair<int, int>> map;
  choices.push_back("All spells");
  map.push_back({0, 0});
  for (int s : skills) {
    choices.push_back(std::string("Skill: ") + skill_name(static_cast<uint8_t>(s)));
    map.push_back({1, s});
  }
  for (int c : cats) {
    choices.push_back("Cat: " + db_string(c, kDbTypeSpellCategory));
    map.push_back({2, c});
  }
  for (int c : subs) {
    choices.push_back("Sub: " + db_string(c, kDbTypeSpellCategory));
    map.push_back({3, c});
  }
  if (choices == g_filter_choices) return;
  g_filter_choices = choices;
  g_filter_map = map;
  combo_delete_all(g_combo_filter);
  for (const std::string &s : g_filter_choices) combo_insert_choice(g_combo_filter, s.c_str());
  int sel = 0;
  for (size_t i = 0; i < g_filter_map.size(); ++i)
    if (g_filter_map[i].first == g_filter_kind && g_filter_map[i].second == g_filter_value) {
      sel = static_cast<int>(i);
      break;
    }
  if (sel == 0 && g_filter_kind != 0) {
    g_filter_kind = 0;  // the active filter's target no longer exists
    g_filter_value = 0;
  }
  combo_set_choice(g_combo_filter, sel);
  g_last_filter_choice = combo_get_cur_choice(g_combo_filter);
}

// Header-click detection: any change of the native sort fields away from the
// normalized state (g_sort_col, asc=1) means the native OnHeaderClick ran. The
// FIRST change of a click decides column/direction; further changes within the
// cooldown are the same physical click re-firing and are swallowed. Every
// observed change forces a rebuild, because the native (text-order) Sort just
// reordered the rows out from under our model.
void sort_poll() {
  if (!g_list) return;
  char *lw = reinterpret_cast<char *>(g_list);
  if (g_sort_cooldown > 0) --g_sort_cooldown;
  const int ncol = *reinterpret_cast<int *>(lw + kListSortColOffset);
  const uint8_t nasc = *reinterpret_cast<uint8_t *>(lw + kListSortAscOffset);
  if (ncol == g_sort_col && nasc == 1) return;  // still normalized: no interaction
  if (ncol > 0 && ncol < kColCount && g_sort_cooldown == 0) {
    if (ncol != g_sort_col) {
      g_sort_col = ncol;
      g_sort_asc = true;
    } else {
      g_sort_asc = !g_sort_asc;
    }
    g_sort_cooldown = 20;  // ~1/3s: swallow this click's extra fires
  }
  *reinterpret_cast<int *>(lw + kListSortColOffset) = g_sort_col;  // re-normalize
  *reinterpret_cast<uint8_t *>(lw + kListSortAscOffset) = 1;
  g_last_sig = 0;  // repaint in OUR order
}

// Draggable list/description split. The drag STARTS from a native press on the
// Rcp_SbSplit handle button (CButtonWnd MouseButtonState@+0x1d8 goes nonzero on
// capture, bMouseOverLastFrame@+0x1e5 while hovered) - native click routing, no
// coordinate-space assumptions. The drag itself tracks RELATIVE game-mouse
// movement, and all rect math is child-vs-child (same coordinate space).
struct WndRect {
  int l, t, r, b;
};
WndRect *wnd_rect(void *w) { return reinterpret_cast<WndRect *>(reinterpret_cast<char *>(w) + kWndLocationOffset); }
// (Resize@0x863990 exists - FIVE args (w,h,1,0,0), ret 0x14 - but it PRESERVES
// the current top-left, so all layout here goes through wnd_move instead.)
// The virtual Resize calls internally: vtable+0x11C = Move(const CXRect&, bool
// bUpdateLayout, bool, bool, bool) - the only correct way to change a window's
// top-left at runtime (direct +0xC0 pokes are overwritten by the next layout).
constexpr int kMoveVtOffset = 0x11C;
void wnd_move(void *w, int l, int t, int r, int b) {
  if (!w) return;
  int rect[4] = {l, t, r, b};
  void *vtable = *reinterpret_cast<void **>(w);
  void *fn = *reinterpret_cast<void **>(reinterpret_cast<char *>(vtable) + kMoveVtOffset);
  reinterpret_cast<void(__thiscall *)(void *, const int *, int, int, int, int)>(fn)(w, rect, 1, 0, 0, 0);
}
constexpr int kBtnMouseStateOffset = 0x1d8;  // CButtonWnd::MouseButtonState
constexpr int kBtnMouseOverOffset = 0x1e5;   // CButtonWnd::bMouseOverLastFrame

void refresh_description();  // forward (re-pushed after a drag ends, to re-wrap)

// Lays out list + handle + description with the given list height. Coordinate
// model (settled by the in-game +0x60 rect dump): the TOP-LEVEL window rect is
// screen-absolute and live; CHILD rects are PARENT-RELATIVE (list at (6,28)
// wherever the window sits). So children are laid out in parent space from the
// window SIZE + margins; only sizes are taken from the window rect.
void apply_layout(int list_h) {
  WndRect *wr = wnd_rect(g_wnd);
  const int win_w = wr->r - wr->l, win_h = wr->b - wr->t;
  const int lx = g_d_left;
  const int rx = win_w - g_d_right;
  const int ly = g_d_ltop;
  const int desc_bottom = win_h - g_d_bottom;
  const int max_h = desc_bottom - kDescMinCY - kSplitGap - ly;
  if (list_h > max_h) list_h = max_h;
  if (list_h < kListMinCY) list_h = kListMinCY;
  if (rx - lx < 120) return;  // absurdly narrow; leave the children alone
  wnd_move(g_list, lx, ly, rx, ly + list_h);
  const int split_top = ly + list_h;
  if (g_btn_split) wnd_move(g_btn_split, lx, split_top, rx, split_top + kSplitGap);
  const int desc_top = split_top + kSplitGap;
  wnd_move(g_stml, lx, desc_top, rx, desc_bottom > desc_top + kDescMinCY ? desc_bottom : desc_top + kDescMinCY);
}

void splitter_poll() {
  if (!g_list || !g_stml || !g_btn_split) return;
  const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  const int my = *reinterpret_cast<int *>(kMouseInfo + 4);
  char *btn = reinterpret_cast<char *>(g_btn_split);
  checkbox_set(g_btn_split, false);  // the handle is not a real checkbox; keep it unlatched
  if (!g_split_drag) {
    const bool pressed = *reinterpret_cast<int *>(btn + kBtnMouseStateOffset) != 0 ||
                         (lmb && *reinterpret_cast<uint8_t *>(btn + kBtnMouseOverOffset) != 0);
    if (lmb && pressed) {
      WndRect *lr = wnd_rect(g_list);
      g_split_drag = true;
      g_split_start_my = my;
      g_split_start_list_h = lr->b - lr->t;
      logger::logf("[book] split drag start: my=%d list_h=%d", my, g_split_start_list_h);
    }
    return;
  }
  if (!lmb) {
    g_split_drag = false;
    refresh_description();  // re-wrap the STML text for the final size
    return;
  }
  apply_layout(g_split_start_list_h + (my - g_split_start_my));
}

void update_status_label(int count, int level) {
  (void)level;
  if (!g_lbl_status) return;
  char b[48];  // keep it short: the label has ~150px between the checkbox and the gauges
  if (g_show_unscribed)
    std::snprintf(b, sizeof(b), "%d unscribed", count);
  else
    std::snprintf(b, sizeof(b), "%d scribed", count);
  set_window_text(g_lbl_status, b);
}

// Evaluate the spell-description placeholder tokens against the real spell
// data: #N = effect base value, $N = effect max value, @N = max-or-base
// (N = effect slot 1..12), %z = the buff duration. Unknown tokens pass through.
std::string eval_desc_tokens(const std::string &in, char *sp, int level) {
  std::string out;
  out.reserve(in.size() + 16);
  for (size_t i = 0; i < in.size();) {
    const char c = in[i];
    if ((c == '#' || c == '$' || c == '@') && i + 1 < in.size() &&
        std::isdigit(static_cast<unsigned char>(in[i + 1]))) {
      size_t j = i + 1;
      int idx = 0;
      while (j < in.size() && std::isdigit(static_cast<unsigned char>(in[j])) && j - i <= 2)
        idx = idx * 10 + (in[j++] - '0');
      if (idx >= 1 && idx <= 12) {
        const int base = *reinterpret_cast<int *>(sp + kSpBase + (idx - 1) * 4);
        const int mx = *reinterpret_cast<int *>(sp + kSpMax + (idx - 1) * 4);
        const int v = c == '$' ? mx : (c == '@' ? (mx ? mx : base) : base);
        char nb[16];
        std::snprintf(nb, sizeof(nb), "%d", v < 0 ? -v : v);
        out += nb;
        i = j;
        continue;
      }
    }
    if (c == '%' && i + 1 < in.size() && (in[i + 1] == 'z' || in[i + 1] == 'Z')) {
      out += fmt_duration(duration_ticks(level, *reinterpret_cast<int *>(sp + kSpDurationType),
                                         *reinterpret_cast<int *>(sp + kSpDurationCap)));
      i += 2;
      continue;
    }
    out += c;
    ++i;
  }
  return out;
}

std::string stml_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else out += c;
  }
  return out;
}

// noinline: keep the heavy rebuild/description bodies OUT of on_frame's frame.
// With everything inlined, on_frame grew past 0x2a00 bytes and a spill-slot
// conflict left a vector's data pointer holding the reserve SIZE (0x800) -> a
// memcpy from address 0x800 = the round-8 open crash. Separate frames give the
// optimizer sane allocation and honest names in crash backtraces.
__attribute__((noinline)) void refresh_description() {
  if (!g_stml) return;
  char *sp = g_sel_spell > 0 ? spell_by_id(g_sel_spell) : nullptr;
  if (!sp) {
    stml_set_text(g_stml, "");
    return;
  }
  char *prof = get_pc_profile();
  const int cls = prof ? *reinterpret_cast<int *>(prof + kProfClass) : 0;
  const int lvl = prof ? *reinterpret_cast<int *>(prof + kProfLevel) : 60;
  std::string cat = db_string(*reinterpret_cast<int *>(sp + kSpCategory), kDbTypeSpellCategory);
  std::string sub = db_string(*reinterpret_cast<int *>(sp + kSpSubcategory), kDbTypeSpellCategory);
  std::string desc = db_string(*reinterpret_cast<int *>(sp + kSpDescriptionIdx), kDbTypeSpellDescription);

  std::string text = "<c \"#FFFF66\">" + stml_escape(sp + kSpName) + "</c>";
  const int lv = spell_class_level(sp, cls);
  char b[160];
  if (lv < 255) {
    std::snprintf(b, sizeof(b), "  (level %d)", lv);
    text += b;
  }
  std::snprintf(b, sizeof(b), "<br>Mana %s   Cast %s   Recast %s   Duration %s   Target %s",
                col_cell_text(kColMana, sp, cls, lvl).c_str(), col_cell_text(kColCast, sp, cls, lvl).c_str(),
                col_cell_text(kColRecast, sp, cls, lvl).c_str(), col_cell_text(kColDur, sp, cls, lvl).c_str(),
                col_cell_text(kColTarget, sp, cls, lvl).c_str());
  text += b;
  if (!cat.empty() || !sub.empty())
    text += "<br>" + stml_escape(cat) + (sub.empty() ? "" : " / " + stml_escape(sub));
  if (!desc.empty()) text += "<br><br>" + stml_escape(eval_desc_tokens(desc, sp, lvl));
  stml_set_text(g_stml, text.c_str());
}

__attribute__((noinline)) void refresh_list() {
  if (!g_list) return;
  char *prof = get_pc_profile();
  if (!prof || !*kSpellMgr) return;
  const int cls = *reinterpret_cast<int *>(prof + kProfClass);
  const int lvl = *reinterpret_cast<int *>(prof + kProfLevel);

  // Row set: the scribed book - or, with "Show unscribed" on, ONLY the spells
  // your class could use at your current level that are NOT in the book.
  // Reserves cover the maximums (720 book slots / every class spell), so the
  // vector growth path never runs mid-loop.
  std::vector<SpellRow> rows;
  rows.reserve(kBookSlots + g_class_spells.size() + 16);
  if (!g_show_unscribed) {
    for (int i = 0; i < kBookSlots; ++i) {
      const int id = *reinterpret_cast<int *>(prof + kProfSpellBook + i * 4);
      char *sp = id > 0 ? spell_by_id(id) : nullptr;
      if (sp && row_visible(sp, cls)) rows.push_back({id, true});
    }
  } else {
    std::vector<int> scribed_ids;
    scribed_ids.reserve(kBookSlots);
    // Best scribed rank per spell FAMILY ("Spell" / "Spell Rk. II/III"), so the
    // unscribed list only offers actual UPGRADES (ranks above what is owned).
    std::map<std::string, int> fam_rank;
    std::string base;
    for (int i = 0; i < kBookSlots; ++i) {
      const int id = *reinterpret_cast<int *>(prof + kProfSpellBook + i * 4);
      if (id <= 0) continue;
      scribed_ids.push_back(id);
      char *sp = spell_by_id(id);
      if (!sp) continue;
      const int rank = spell_rank_and_base(sp + kSpName, base);
      int &best = fam_rank[base];
      if (rank > best) best = rank;
    }
    std::sort(scribed_ids.begin(), scribed_ids.end());
    ensure_class_spells(cls);
    for (int id : g_class_spells) {
      char *sp = spell_by_id(id);
      if (!sp || !row_visible(sp, cls)) continue;
      if (spell_class_level(sp, cls) > lvl) continue;  // only spells you could use at your level
      if (std::binary_search(scribed_ids.begin(), scribed_ids.end(), id)) continue;
      const int rank = spell_rank_and_base(sp + kSpName, base);
      const auto it = fam_rank.find(base);
      if (it != fam_rank.end() && rank <= it->second) continue;  // owned rank covers this one
      rows.push_back({id, false});
    }
  }

  // Collect the filter-picker universe from the UNFILTERED rows, then apply
  // the live search (case-insensitive name substring) + the picked filter.
  std::vector<int> f_skills, f_cats, f_subs;
  f_skills.reserve(rows.size());
  f_cats.reserve(rows.size());
  f_subs.reserve(rows.size());
  {
    std::vector<SpellRow> kept;
    kept.reserve(rows.size());
    for (const SpellRow &r : rows) {
      char *sp = spell_by_id(r.spell_id);
      if (!sp) continue;
      f_skills.push_back(*reinterpret_cast<uint8_t *>(sp + kSpSkill));
      f_cats.push_back(*reinterpret_cast<int *>(sp + kSpCategory));
      f_subs.push_back(*reinterpret_cast<int *>(sp + kSpSubcategory));
      if (!g_search.empty() && to_lower(sp + kSpName).find(g_search) == std::string::npos) continue;
      if (!filter_matches(sp)) continue;
      kept.push_back(r);
    }
    rows.swap(kept);
  }
  populate_filter_combo(std::move(f_skills), std::move(f_cats), std::move(f_subs));

  // Sort by OUR sort state (numeric-aware; sort_poll owns the header clicks and
  // any native text-order Sort is overwritten by this full repaint).
  const int sort_col = g_sort_col > 0 && g_sort_col < kColCount ? g_sort_col : kColName;
  const bool asc = g_sort_asc;
  struct Ent {
    SpellRow row;
    long long num;
    std::string str;
    std::string name;
  };
  std::vector<Ent> ents;
  ents.reserve(rows.size());
  for (const SpellRow &r : rows) {
    char *sp = spell_by_id(r.spell_id);
    if (!sp) continue;
    Ent e;
    e.row = r;
    e.name = to_lower(sp + kSpName);
    if (col_is_numeric(sort_col))
      e.num = col_num_key(sort_col, sp, cls, lvl);
    else
      e.str = sort_col == kColName ? e.name : to_lower(col_cell_text(sort_col, sp, cls, lvl));
    ents.push_back(std::move(e));
  }
  const bool numeric = col_is_numeric(sort_col);
  std::stable_sort(ents.begin(), ents.end(), [&](const Ent &a, const Ent &b) {
    if (numeric) {
      if (a.num != b.num) return asc ? a.num < b.num : a.num > b.num;
    } else {
      if (a.str != b.str) return asc ? a.str < b.str : a.str > b.str;
    }
    return a.name < b.name;  // stable tie-break: name ascending
  });

  // Repaint.
  list_clear(g_list);
  g_rows.clear();
  g_rows.reserve(ents.size());
  for (const Ent &e : ents) {
    char *sp = spell_by_id(e.row.spell_id);
    if (!sp) continue;
    void *icon = icon_for_cell(*reinterpret_cast<int *>(sp + kSpSpellIcon));
    const uint32_t argb = e.row.scribed ? 0xFFFFFFFFu : 0xFF808080u;
    const int row = list_add_row(g_list, "", argb, static_cast<uint32_t>(e.row.spell_id), icon);
    if (row < 0) continue;
    for (int c = kColName; c < kColCount; ++c) {
      list_set_item_text(g_list, row, c, col_cell_text(c, sp, cls, lvl).c_str());
      list_set_item_color(g_list, row, c, argb);
    }
    g_rows.push_back(e.row);
  }
  update_status_label(static_cast<int>(g_rows.size()), lvl);

  // Restore the selection (by spell id, never row index) + keep it on screen.
  int sel_row = -1;
  for (size_t i = 0; i < g_rows.size(); ++i)
    if (g_rows[i].spell_id == g_sel_spell) {
      sel_row = static_cast<int>(i);
      break;
    }
  list_set_cur_sel(g_list, sel_row);
  if (sel_row >= 0) list_ensure_visible(g_list, sel_row);
  g_last_sel = list_get_cur_sel(g_list);
  g_last_col = *reinterpret_cast<int *>(reinterpret_cast<char *>(g_list) + kListCurColOffset);
}

// ---- window lifecycle ----
void create_window() {
  g_create_attempted = true;
  void *sidlmgr = *kSidlManager;
  if (!sidlmgr) {
    logger::log("[book] create_window: SIDL manager is null");
    return;
  }
  uint32_t name_cxstr;
  cxstr_init(&name_cxstr, "RcpSpellBook");
  g_wnd = reinterpret_cast<void *(__thiscall *)(void *, void *, void *)>(kCreateXWnd)(sidlmgr, nullptr, &name_cxstr);
  logger::logf("[book] CreateXWndFromTemplate('RcpSpellBook') = %p", g_wnd);
  if (!g_wnd) return;
  g_list = get_child(g_wnd, "Rcp_SbList");
  g_edit = get_child(g_wnd, "Rcp_SbSearch");
  g_cb_unscribed = get_child(g_wnd, "Rcp_SbUnscribed");
  g_lbl_status = get_child(g_wnd, "Rcp_SbStatus");
  g_stml = get_child(g_wnd, "Rcp_SbDesc");
  g_btn_split = get_child(g_wnd, "Rcp_SbSplit");
  g_combo_filter = get_child(g_wnd, "Rcp_SbFilter");
  logger::logf("[book] controls bound: list=%p edit=%p cb=%p status=%p stml=%p split=%p filter=%p", g_list,
               g_edit, g_cb_unscribed, g_lbl_status, g_stml, g_btn_split, g_combo_filter);
  if (g_list) {
    char *lw = reinterpret_cast<char *>(g_list);
    // Fixed row height tall enough for the icon cell (tune in-game if needed).
    *reinterpret_cast<int *>(lw + kListLineHeightOffset) = 24;
    *reinterpret_cast<uint8_t *>(lw + kListFixedHeightOffset) = 1;
    // Arm the native header: the sort bits make OnHeaderClick fire (sort_poll
    // uses it purely as a click detector), and SetColumnsSizable enables
    // drag-resizing the column separators.
    *reinterpret_cast<uint32_t *>(lw + kListStyleOffset) |= kListStyleSortable;
    reinterpret_cast<void(__thiscall *)(void *, int)>(kListSetColumnsSizable)(g_list, 1);
    // Seed the normalized native state + our own sort state (level ascending).
    g_sort_col = kColLvl;
    g_sort_asc = true;
    g_sort_cooldown = 0;
    *reinterpret_cast<int *>(lw + kListSortColOffset) = kColLvl;
    *reinterpret_cast<uint8_t *>(lw + kListSortAscOffset) = 1;
  }
  checkbox_set(g_cb_unscribed, g_show_unscribed);
  g_last_unscribed_cb = g_show_unscribed;
  // Column widths are safe to restore now; ALL rect-based geometry (margins
  // capture, window pos/size restore, split restore) waits until the child
  // Location rects are populated - they are ZEROS until the first draw. See
  // try_anchor(), which runs from the frame poll once the window is visible.
  for (int i = 0; i < kGeoCols; ++i) list_set_col_width(g_list, i, g_geo_colw[i]);
  g_anchored = false;
  g_lay_w = 0;
  g_lay_h = 0;
  // Restore the saved split - unless it is the collapsed-minimum a mis-layout
  // once persisted; the XML default is better than a pinned 96px list.
  g_pending_listcy = g_geo_listcy > kListMinCY + 10 ? g_geo_listcy : 0;
  show_window(g_wnd, false);  // created hidden; /rcpbook or the redirect reveals it
}

bool rect_sane(const WndRect *r) {
  return r->l > -8000 && r->r > r->l && r->r - r->l < 8000 && r->t > -8000 && r->b > r->t && r->b - r->t < 8000;
}

// Once the freshly created window has valid rects, capture the design margins
// (CHILD rects are parent-relative), then apply the persisted window rect and
// split. Until this has run, no other geometry code touches the rects.
void try_anchor() {
  if (!g_list || !g_stml) return;
  WndRect *wr = wnd_rect(g_wnd);
  WndRect *lr = wnd_rect(g_list);
  WndRect *dr = wnd_rect(g_stml);
  static int s_log_cd = 0;
  if (--s_log_cd <= 0) {
    s_log_cd = 120;
    logger::logf("[book] rects: win %d,%d-%d,%d list %d,%d-%d,%d desc %d,%d-%d,%d", wr->l, wr->t, wr->r, wr->b,
                 lr->l, lr->t, lr->r, lr->b, dr->l, dr->t, dr->r, dr->b);
  }
  if (!rect_sane(wr) || !rect_sane(lr) || !rect_sane(dr)) return;
  // Child rects are parent-relative: the list's coords ARE the design margins
  // (6,28). Right margin mirrors the left (symmetric design); the bottom gap is
  // the design constant (generator: WINDOW_CY - desc bottom = 4).
  const int d_left = lr->l;
  const int d_ltop = lr->t;
  if (d_left < 0 || d_left > 100 || d_ltop < 0 || d_ltop > 150) return;  // not laid out yet (logged above)
  g_d_left = d_left;
  g_d_ltop = d_ltop;
  g_d_right = d_left;
  g_d_bottom = 4;
  g_anchored = true;
  logger::logf("[book] anchored: left=%d top=%d right=%d bottom=%d", g_d_left, g_d_ltop, g_d_right, g_d_bottom);
  // Apply the persisted window rect now, then the persisted split.
  const bool have_pos = g_geo_winx > -100000 && g_geo_winy > -100000;
  const bool have_size = g_geo_winw >= 400 && g_geo_winh >= 200;
  if (have_pos || have_size) {
    const int x = have_pos ? g_geo_winx : wr->l;
    const int y = have_pos ? g_geo_winy : wr->t;
    const int w = have_size ? g_geo_winw : wr->r - wr->l;
    const int h = have_size ? g_geo_winh : wr->b - wr->t;
    wnd_move(g_wnd, x, y, x + w, y + h);
  }
  apply_layout(g_pending_listcy > 0 ? g_pending_listcy : wnd_rect(g_list)->b - wnd_rect(g_list)->t);
  g_pending_listcy = 0;
  g_lay_w = 0;  // let layout_poll take a fresh baseline next frame
}

// Persist geometry when it changes (checked every ~1.5s while the window is
// visible, so border/column drags produce one batch of ini writes at rest).
void geometry_poll() {
  if (--g_geo_poll_cd > 0) return;
  g_geo_poll_cd = 90;
  WndRect *wr = wnd_rect(g_wnd);
  WndRect *lr = wnd_rect(g_list);
  bool dirty = false;
  auto upd = [&](int &slot, int v) {
    if (slot != v) {
      slot = v;
      dirty = true;
    }
  };
  upd(g_geo_winx, wr->l);
  upd(g_geo_winy, wr->t);
  upd(g_geo_winw, wr->r - wr->l);
  upd(g_geo_winh, wr->b - wr->t);
  upd(g_geo_listcy, lr->b - lr->t);
  for (int i = 0; i < kGeoCols; ++i) {
    const int cw = list_get_col_width(g_list, i);
    if (cw > 0) upd(g_geo_colw[i], cw);
  }
  if (dirty) save_geometry();
}

// Re-anchor the children whenever the window rect changes (resize from ANY
// edge, or a move). The list keeps its height (the user's chosen split) unless
// the window gets too short for it.
void layout_poll() {
  if (!g_list || !g_stml || !g_anchored) return;
  WndRect *wr = wnd_rect(g_wnd);
  if (wr->l == g_lay_l && wr->t == g_lay_t && wr->r - wr->l == g_lay_w && wr->b - wr->t == g_lay_h) return;
  g_lay_l = wr->l;
  g_lay_t = wr->t;
  g_lay_w = wr->r - wr->l;
  g_lay_h = wr->b - wr->t;
  WndRect *lr = wnd_rect(g_list);
  apply_layout(lr->b - lr->t);
  refresh_description();  // re-wrap for the new width
}

void toggle_window() {
  if (!g_wnd && !g_create_attempted) create_window();
  if (!g_wnd) {
    Rcp::Game::print_chat("rof2ClientPlus spell book window unavailable (is uifiles/default installed? re-run make install).");
    return;
  }
  const bool make_visible = !is_visible(g_wnd);
  if (make_visible) {
    checkbox_set(g_cb_unscribed, g_show_unscribed);
    g_last_unscribed_cb = g_show_unscribed;
    g_last_sig = 0;  // force a rebuild on the next frame poll
  }
  show_window(g_wnd, make_visible);
  if (make_visible) {
    // Give the search box keyboard focus so typing filters immediately, and
    // request a sit - memorizing (the whole point of the window) requires it.
    // The sit itself is DEFERRED to the next frame (see g_want_sit).
    if (g_edit) reinterpret_cast<void *(__thiscall *)(void *)>(kSetFocus)(g_edit);
    g_want_sit = true;
    g_open_grace = 40;  // suspend the close-on-stand check while the sit lands
  }
}

// ---- memorize (cursor pickup) ----
// The client's own busy gate (StartSpellMemorization@0x75BF30 requires both -1).
bool mem_in_progress() {
  char *sb = static_cast<char *>(*kSpellBookWnd);
  if (!sb) return false;
  return *reinterpret_cast<int *>(sb + kBookMemSlotOffset) != -1 ||
         *reinterpret_cast<int *>(sb + kBookScribeSlotOffset) != -1;
}

// Replicates the stock spell pickup: attach the spell to the cursor as a
// memorize-drag (type 1, index = book slot); the native gem drop runs
// StartSpellMemorization from there.
void try_pickup(int spell_id) {
  char *sp = spell_by_id(spell_id);
  if (!sp) return;
  void *cursor = *kCursorAttach;
  char *sb = static_cast<char *>(*kSpellBookWnd);
  logger::logf("[book] pickup: spell=%d '%s' cursor=%p type=%d book=%p memslot=%d scribeslot=%d", spell_id,
               sp + kSpName, cursor,
               cursor ? *reinterpret_cast<int *>(static_cast<char *>(cursor) + kCursorTypeOffset) : -99,
               (void *)sb, sb ? *reinterpret_cast<int *>(sb + kBookMemSlotOffset) : -99,
               sb ? *reinterpret_cast<int *>(sb + kBookScribeSlotOffset) : -99);
  if (!cursor) {
    Rcp::Game::print_chat("rof2ClientPlus: cursor-attachment window not found (see log).");
    return;
  }
  if (!reinterpret_cast<bool(__thiscall *)(void *, int)>(kIsOkToActivate)(cursor, kCursorAttachMemorizeSpell)) {
    Rcp::Game::print_chat("rof2ClientPlus: your cursor is busy - drop what it holds first.");
    logger::log("[book] pickup: IsOkToActivate said no");
    return;
  }
  if (mem_in_progress()) {
    Rcp::Game::print_chat("rof2ClientPlus: already memorizing a spell.");
    logger::log("[book] pickup: mem_in_progress gate");
    return;
  }
  char *prof = get_pc_profile();
  if (!prof) return;
  int book_slot = -1;
  for (int i = 0; i < kBookSlots; ++i)
    if (*reinterpret_cast<int *>(prof + kProfSpellBook + i * 4) == spell_id) {
      book_slot = i;
      break;
    }
  if (book_slot < 0) {
    Rcp::Game::print_chat("rof2ClientPlus: %s is not in your spell book (unscribed spells cannot be memorized).",
                          sp + kSpName);
    return;
  }
  // The overlay is REQUIRED for type 1 (see the constants comment) and is what
  // the cursor draws; our long-lived icon copy is safe to hand over.
  void *overlay = icon_for_cell(*reinterpret_cast<int *>(sp + kSpSpellIcon));
  if (!overlay) {
    logger::log("[book] pickup: no A_SpellIcons animation for the overlay");
    return;
  }
  reinterpret_cast<void(__thiscall *)(void *, void *, void *, int, int, const char *, int)>(kAttachToCursor6)(
      cursor, overlay, nullptr, kCursorAttachMemorizeSpell, book_slot, nullptr, -1);
  const int type = *reinterpret_cast<int *>(static_cast<char *>(cursor) + kCursorTypeOffset);
  if (type == kCursorAttachMemorizeSpell) {
    Rcp::Game::print_chat("Picked up %s - click a spell gem to memorize it.", sp + kSpName);
  } else {
    Rcp::Game::print_chat("rof2ClientPlus: could not pick up %s (see log).", sp + kSpName);
    logger::logf("[book] pickup failed: spell=%d bookslot=%d attach-type=%d", spell_id, book_slot, type);
  }
}

// ---- right-click-to-scribe ----
// The cursor item, fetched exactly like every stock call site in
// BeginSpellScribe does (vbase container chain + GetItem(33)). GetItem addrefs;
// the extra reference is dropped immediately - the inventory keeps its own, so
// the pointer stays valid for synchronous use (equip_item.cpp pattern).
void *get_cursor_item() {
  char *pc = *kLocalPC;
  if (!pc) return nullptr;
  char *vbase = *reinterpret_cast<char **>(pc + 8);
  if (!vbase) return nullptr;
  char *container = pc + 8 + *reinterpret_cast<int *>(vbase + 4);
  void *out[2] = {nullptr, nullptr};
  reinterpret_cast<void *(__thiscall *)(void *, void *, int)>(kContainerGetItem)(container, out, kInvSlotCursor);
  void *item = out[0];
  if (item)
    InterlockedDecrement(reinterpret_cast<volatile LONG *>(reinterpret_cast<char *>(item) + kOffItemRefCount));
  return item;
}

int item_type(void *item) {
  return item ? reinterpret_cast<int(__thiscall *)(void *)>(kItemGetType)(item) : 0;
}
int item_scroll_spell(void *item) {
  return item ? reinterpret_cast<int(__thiscall *)(void *, int)>(kItemGetSpellId)(item, kItemSpellTypeScroll) : 0;
}
bool spell_known(int spell_id) {
  char *pc = *kLocalPC;
  if (!pc) return false;
  return reinterpret_cast<int(__thiscall *)(void *, int)>(kSpellBookKnows)(pc + kSpellBookInfoOffset, spell_id) != 0;
}

// The client's full busy gate for starting a mem/scribe: both book slots idle
// AND no request packet in flight (BeginSpellScribe checks all three).
bool scribe_busy() { return mem_in_progress() || *kBookRequestsInFlight > 0; }

// The book slot to scribe into: the slot of a LOWER-rank same-group spell if
// one is scribed (BeginSpellScribe then runs its native replace-lower-rank
// confirm dialog against that slot), else the first empty slot. Returns -1
// when the book is full; sets *blocked when an equal/higher rank of the same
// SpellGroup is already known (the native flow would refuse after the pickup).
int pick_scribe_slot(char *prof, char *sp, bool *blocked) {
  *blocked = false;
  const int group = *reinterpret_cast<int *>(sp + kSpSpellGroup);
  const int rank = *reinterpret_cast<int *>(sp + kSpSpellRank);
  int first_empty = -1, upgrade_slot = -1;
  for (int i = 0; i < kBookSlots; ++i) {
    const int id = *reinterpret_cast<int *>(prof + kProfSpellBook + i * 4);
    if (id <= 0) {
      if (first_empty < 0) first_empty = i;
      continue;
    }
    if (group == 0) continue;
    char *bs = spell_by_id(id);
    if (!bs || *reinterpret_cast<int *>(bs + kSpSpellGroup) != group) continue;
    if (*reinterpret_cast<int *>(bs + kSpSpellRank) >= rank) {
      *blocked = true;
      return -1;
    }
    if (upgrade_slot < 0) upgrade_slot = i;
  }
  return upgrade_slot >= 0 ? upgrade_slot : first_empty;
}

// Frame poll half of the right-click scribe: the scroll is (about to be) on the
// cursor; open whichever book is active and hand the slot to the client's own
// BeginSpellScribe (which prints "You begin scribing..." and seeds the
// countdown our Rcp_SbScribeGauge - or the stock book's gauge - then ticks).
void scribe_poll() {
  if (!g_pending_scribe_spell) return;
  char *sb = static_cast<char *>(*kSpellBookWnd);
  if (!sb || !Rcp::Game::is_in_game()) {
    g_pending_scribe_spell = 0;
    return;
  }
  if (--g_pending_scribe_frames < 0) {
    logger::logf("[book] scribe: cursor never got spell %d - giving up", g_pending_scribe_spell);
    g_pending_scribe_spell = 0;
    return;
  }
  if (scribe_busy()) {  // a memorize slipped in between the click and this frame
    Rcp::Game::print_chat("rof2ClientPlus: cannot scribe - already memorizing or scribing a spell.");
    g_pending_scribe_spell = 0;
    return;
  }
  // Wait (a frame, normally zero) for the MoveItem to land the scroll on the cursor.
  void *cur = get_cursor_item();
  if (!cur || item_scroll_spell(cur) != g_pending_scribe_spell) return;

  // Open the active book flavor. Ours opens anywhere (and requests the sit);
  // the stock book's own Activate runs its native AboutToShow gates (auto-sit,
  // not-moving, not-looting) and leaves it hidden when they refuse.
  if (g_enabled) {
    if (!g_wnd || !is_visible(g_wnd)) toggle_window();
    if (!g_wnd || !is_visible(g_wnd)) {
      g_pending_scribe_spell = 0;  // toggle_window printed why it is unavailable
      return;
    }
  } else {
    if (!is_visible(sb)) wnd_call_void(sb, kActivateVtOffset);
    if (!is_visible(sb)) {
      Rcp::Game::print_chat("rof2ClientPlus: could not open the spell book to scribe.");
      g_pending_scribe_spell = 0;
      return;
    }
  }

  char *prof = get_pc_profile();
  char *sp = spell_by_id(g_pending_scribe_spell);
  const int spell_id = g_pending_scribe_spell;
  g_pending_scribe_spell = 0;
  if (!prof || !sp) return;
  bool blocked = false;
  const int slot = pick_scribe_slot(prof, sp, &blocked);
  if (blocked || slot < 0) {  // book changed since the click (server update)
    Rcp::Game::print_chat("rof2ClientPlus: no spell book slot available for %s.", sp + kSpName);
    return;
  }
  reinterpret_cast<void(__thiscall *)(void *, int)>(kBookBeginScribe)(sb, slot);
  const int active_slot = *reinterpret_cast<int *>(sb + kBookScribeSlotOffset);
  logger::logf("[book] scribe begin: spell=%d '%s' slot=%d -> scribeslot=%d (rcp window=%d)", spell_id, sp + kSpName,
               slot, active_slot, (int)g_enabled);
  // Stock book: turn to the spread that holds the slot being scribed.
  if (!g_enabled && active_slot == slot)
    reinterpret_cast<void(__thiscall *)(void *, int)>(kBookTurnToPage)(sb, (slot / 8) & ~1);
  // If BeginSpellScribe refused it printed its own native message (or popped
  // the replace-lower-rank confirm dialog, which continues the flow itself).
}

// Closing the new book window - directly, via the book-key toggle, or by the
// close-on-stand poll - must abandon a running memorize/scribe exactly like
// closing the stock book does. The stock close path is Show(false) ->
// AboutToHide -> the hide worker; the stock book is already hidden while our
// window drives the gauges, so the worker is called directly on the edge.
void scribe_hide_edge_poll() {
  const bool vis = g_wnd && is_visible(g_wnd);
  if (g_book_was_visible && !vis && mem_in_progress()) {
    char *sb = static_cast<char *>(*kSpellBookWnd);
    if (sb) {
      logger::logf("[book] window closed with memslot=%d scribeslot=%d - running the stock hide-cancel",
                   *reinterpret_cast<int *>(sb + kBookMemSlotOffset),
                   *reinterpret_cast<int *>(sb + kBookScribeSlotOffset));
      reinterpret_cast<void(__thiscall *)(void *)>(kBookHideCancel)(sb);
    }
  }
  g_book_was_visible = vis;
}

// ---- stock-book redirect ----
// While enabled, any path that opens the stock spell book (keybind, book button,
// /book) is caught here on the visibility edge: the stock window is hidden and
// OURS is toggled instead - so the book key behaves as a toggle for the new
// window too. The one exception is the client's own memorize/scribe animation,
// which needs the stock book visible until its countdown finishes; that open is
// swallowed without toggling ours.
void suppression_poll() {
  void *stock_book = *kSpellBookWnd;
  if (!stock_book) return;

  // While a memorize/scribe runs, leave the stock book alone (a manually
  // opened book keeps its animation; a hidden one STAYS hidden - the countdown
  // is driven by OUR EQType-9/10 gauges, so the book is not needed at all).
  if (mem_in_progress()) {
    g_mem_was_open = is_visible(stock_book);
    return;
  }

  if (!g_enabled) return;
  if (is_visible(stock_book)) {
    // Proper close path (clears the active flag), then make sure it is hidden.
    wnd_call_void(stock_book, kDeactivateVtOffset);
    show_window(stock_book, false);
    if (g_mem_was_open) {
      g_mem_was_open = false;  // the book was only open for the mem animation
    } else {
      toggle_window();
    }
  } else {
    g_mem_was_open = false;
  }
}

// ---- debug helpers (/rcpbook subcommands) ----
void debug_dump() {
  char *prof = get_pc_profile();
  logger::logf("[book] DUMP: pLocalPC=%p profile=%p spellmgr=%p dbstr=%p", (void *)*kLocalPC, (void *)prof,
               (void *)*kSpellMgr, *kDbStr);
  if (!prof) {
    Rcp::Game::print_chat("rof2ClientPlus: profile walk FAILED (see log).");
    return;
  }
  const int cls = *reinterpret_cast<int *>(prof + kProfClass);
  const int lvl = *reinterpret_cast<int *>(prof + kProfLevel);
  logger::logf("[book] DUMP: class=%d level=%d", cls, lvl);
  int scribed = 0, first[10], nfirst = 0;
  for (int i = 0; i < kBookSlots; ++i) {
    const int id = *reinterpret_cast<int *>(prof + kProfSpellBook + i * 4);
    if (id > 0) {
      ++scribed;
      if (nfirst < 10) first[nfirst++] = id;
    }
  }
  logger::logf("[book] DUMP: book has %d scribed; first %d ids:", scribed, nfirst);
  for (int i = 0; i < nfirst; ++i) {
    char *sp = spell_by_id(first[i]);
    if (!sp) {
      logger::logf("[book] DUMP:   id=%d -> NULL spell", first[i]);
      continue;
    }
    logger::logf("[book] DUMP:   id=%d '%s' lvl=%d mana=%d cast=%ums recast=%ums durF=%d durCap=%d icon=%d "
                 "cat=%d('%s') sub=%d('%s')",
                 first[i], sp + kSpName, spell_class_level(sp, cls), *reinterpret_cast<int *>(sp + kSpManaCost),
                 *reinterpret_cast<uint32_t *>(sp + kSpCastTime), *reinterpret_cast<uint32_t *>(sp + kSpRecastTime),
                 *reinterpret_cast<int *>(sp + kSpDurationType), *reinterpret_cast<int *>(sp + kSpDurationCap),
                 *reinterpret_cast<int *>(sp + kSpSpellIcon), *reinterpret_cast<int *>(sp + kSpCategory),
                 db_string(*reinterpret_cast<int *>(sp + kSpCategory), kDbTypeSpellCategory).c_str(),
                 *reinterpret_cast<int *>(sp + kSpSubcategory),
                 db_string(*reinterpret_cast<int *>(sp + kSpSubcategory), kDbTypeSpellCategory).c_str());
    std::string desc = db_string(*reinterpret_cast<int *>(sp + kSpDescriptionIdx), kDbTypeSpellDescription);
    logger::logf("[book] DUMP:     desc: %.100s", desc.c_str());
  }
  Rcp::Game::print_chat("rof2ClientPlus: spell book dump written to the log (class %d, %d scribed).", cls, scribed);
}

void debug_gems() {
  char *prof = get_pc_profile();
  if (!prof) {
    Rcp::Game::print_chat("rof2ClientPlus: profile walk FAILED.");
    return;
  }
  for (int g = 0; g < kNumGems; ++g) {
    const int id = *reinterpret_cast<int *>(prof + kProfMemorized + g * 4);
    char *sp = id > 0 ? spell_by_id(id) : nullptr;
    Rcp::Game::print_chat("gem %d: %d %s", g + 1, id, sp ? sp + kSpName : "(empty)");
  }
}

void debug_findwnd() {
  // POD-collect the registry entries under guard, then log outside it.
  struct Rec {
    void *wnd;
    char name[48];
  };
  static Rec recs[256];
  int count = 0;
  rcp_guard::run("spellbook.findwnd_dump", [&]() {
    char *disp = *kDisplay;
    if (!disp) return;
    char *mgr = disp + kGameScreensOffset;
    const int len = *reinterpret_cast<int *>(mgr);
    char *arr = *reinterpret_cast<char **>(mgr + 4);
    if (!arr || len <= 0 || len > 4096) return;
    for (int i = 0; i < len && count < 256; ++i) {
      char **ppwnd = *reinterpret_cast<char ***>(arr + i * kScreenRecSize);
      if (!ppwnd || !*ppwnd) continue;
      char *rep = *reinterpret_cast<char **>(*ppwnd + kSidlTextOffset);
      if (!rep || reinterpret_cast<uintptr_t>(rep) < 0x10000) continue;
      const uint32_t slen = *reinterpret_cast<uint32_t *>(rep + kRepLength);
      const uint32_t enc = *reinterpret_cast<uint32_t *>(rep + kRepEncoding);
      if (enc != 0 || slen == 0 || slen > 47) continue;
      recs[count].wnd = *ppwnd;
      std::memcpy(recs[count].name, rep + kRepData, slen);
      recs[count].name[slen] = 0;
      ++count;
    }
  });
  logger::logf("[book] FINDWND: %d registered game screens:", count);
  for (int i = 0; i < count; ++i) logger::logf("[book] FINDWND:   %p '%s'", recs[i].wnd, recs[i].name);
  Rcp::Game::print_chat("rof2ClientPlus: %d game screens written to the log (SpellBookWnd=%p CursorAttachment=%p).",
                        count, find_game_screen("SpellBookWnd"), find_game_screen("CursorAttachment"));
}

void print_status() {
  char *sb = static_cast<char *>(*kSpellBookWnd);
  char *cur = static_cast<char *>(*kCursorAttach);
  Rcp::Game::print_chat(
      "rof2ClientPlus spell book: %s (window %s, %d rows; stock book %p memslot %d scribeslot %d active %d%s; "
      "cursor %p type %d)",
      g_enabled ? "NEW window replaces stock" : "stock (new window via /rcpbook only)",
      g_wnd ? (is_visible(g_wnd) ? "open" : "hidden") : "not created", (int)g_rows.size(), (void *)sb,
      sb ? *reinterpret_cast<int *>(sb + kBookMemSlotOffset) : -99,
      sb ? *reinterpret_cast<int *>(sb + kBookScribeSlotOffset) : -99,
      sb ? (int)*reinterpret_cast<uint8_t *>(sb + kWndActiveFlagOffset) : -99,
      mem_in_progress() ? " MEMORIZING" : "", (void *)cur,
      cur ? *reinterpret_cast<int *>(cur + kCursorTypeOffset) : -99);
  if (g_list && g_stml) {
    WndRect *lr = wnd_rect(g_list);
    WndRect *dr = wnd_rect(g_stml);
    Rcp::Game::print_chat("splitter: list rect %d,%d-%d,%d desc rect %d,%d-%d,%d mouse %d,%d standstate %d",
                          lr->l, lr->t, lr->r, lr->b, dr->l, dr->t, dr->r, dr->b,
                          *reinterpret_cast<int *>(kMouseInfo), *reinterpret_cast<int *>(kMouseInfo + 4),
                          (int)player_standstate());
  }
}

}  // namespace

// ---- options-UI / command accessors ----
namespace spellbook_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  save_settings();
}
bool get_show_unscribed() { return g_show_unscribed; }
void set_show_unscribed(bool on) {
  g_show_unscribed = on;
  save_settings();
}
}  // namespace spellbook_settings

namespace spellbook_scribe {
bool get_enabled() { return g_scribe_enabled; }
void set_enabled(bool on) {
  g_scribe_enabled = on;
  save_settings();
}

// Detour half of the right-click scribe (runs inside CInvSlot::HandleRButtonUp
// via equip_item.cpp). Everything BeginSpellScribe would refuse AFTER the
// pickup is pre-checked here so a doomed attempt never strands the scroll on
// the cursor; on success the scroll is moved onto the cursor (where both the
// client's scribe and the server-side completion expect it) and the rest is
// queued for the frame poll (no window opens from inside the click handler).
bool handle_inventory_rclick(void *item, int location, const short slots[3]) {
  if (!g_scribe_enabled || !item || !Rcp::Game::is_in_game()) return false;
  if (item_type(item) != kItemTypeScroll) return false;  // not a spell scroll -> equip/native
  const int spell_id = item_scroll_spell(item);
  char *sp = spell_by_id(spell_id);
  if (spell_id <= 0 || !sp) return false;
  if (*reinterpret_cast<uint8_t *>(sp + kSpIsSkill)) return false;  // BeginSpellScribe silently refuses these
  logger::logf("[book] scribe rclick: item=%p spell=%d '%s' loc=%d slots=%d/%d/%d", item, spell_id, sp + kSpName,
               location, slots[0], slots[1], slots[2]);
  // From here on the click IS a scribe attempt: consume it with feedback.
  char *prof = get_pc_profile();
  if (!prof) return false;
  if (*reinterpret_cast<int *>(reinterpret_cast<char *>(item) + kItemStackCountOffset) > 1) {
    Rcp::Game::print_chat("rof2ClientPlus: pick up a single %s to scribe it (stacked scrolls cannot be scribed).",
                          sp + kSpName);
    return true;
  }
  const int cls = *reinterpret_cast<int *>(prof + kProfClass);
  const int lvl = *reinterpret_cast<int *>(prof + kProfLevel);
  const int req = spell_class_level(sp, cls);
  if (req == 255) {
    Rcp::Game::print_chat("rof2ClientPlus: your class cannot scribe %s.", sp + kSpName);
    return true;
  }
  if (req > lvl) {
    Rcp::Game::print_chat("rof2ClientPlus: you must be level %d to scribe %s.", req, sp + kSpName);
    return true;
  }
  if (spell_known(spell_id)) {
    Rcp::Game::print_chat("You already know %s.", sp + kSpName);
    return true;
  }
  if (scribe_busy()) {
    Rcp::Game::print_chat("rof2ClientPlus: cannot scribe - already memorizing or scribing a spell.");
    return true;
  }
  if (get_cursor_item()) {
    Rcp::Game::print_chat("rof2ClientPlus: your cursor is busy - drop what it holds first.");
    return true;
  }
  bool blocked = false;
  const int slot = pick_scribe_slot(prof, sp, &blocked);
  if (blocked) {
    Rcp::Game::print_chat("You already know an equal or higher rank of %s.", sp + kSpName);
    return true;
  }
  if (slot < 0) {
    Rcp::Game::print_chat("rof2ClientPlus: your spell book is full - no slot for %s.", sp + kSpName);
    return true;
  }
  void *mgr = *kInvSlotMgr;
  if (!mgr) return false;
  ScribeGlobalIndex src = {location, {slots[0], slots[1], slots[2]}, -1};
  ScribeGlobalIndex dst = {location, {kInvSlotCursor, -1, -1}, -1};
  reinterpret_cast<bool(__thiscall *)(void *, const void *, const void *, bool, bool, bool, bool)>(
      kInvSlotMgrMoveItem)(mgr, &src, &dst, false, true, false, false);
  g_pending_scribe_spell = spell_id;
  g_pending_scribe_frames = 30;  // ~half a second of retries for the cursor move
  logger::logf("[book] scribe queued: spell=%d target slot=%d", spell_id, slot);
  return true;
}
}  // namespace spellbook_scribe

void SpellBookUI::drop_handles() {
  g_wnd = g_list = g_edit = g_cb_unscribed = g_lbl_status = g_stml = g_btn_split = g_combo_filter = nullptr;
  g_filter_choices.clear();
  g_filter_map.clear();
  g_last_filter_choice = -1;  // g_filter_kind/value survive: re-selected on repopulate
  g_mem_was_open = false;
  g_split_drag = false;
  g_anchored = false;
  g_create_attempted = false;
  g_pending_scribe_spell = 0;
  g_book_was_visible = false;
  g_rows.clear();
  g_class_spells.clear();
  g_class_cache = -1;
  g_last_sig = 0;
  g_last_sel = -1;
  free_icon_copies();
}

void SpellBookUI::on_frame() {
  if (!Rcp::Game::is_in_game()) {
    if (g_wnd || !g_icon_copies.empty()) drop_handles();
    return;
  }

  suppression_poll();  // hide the stock book + toggle ours (when enabled)

  // Deferred sit request from toggle_window (safe here: ProcessGameEvents
  // context, never nested inside the command interpreter).
  if (g_want_sit) {
    g_want_sit = false;
    sit_if_needed();
  }

  scribe_hide_edge_poll();  // closing our window abandons a mem/scribe (stock parity)
  scribe_poll();            // pending right-click scribe: open the book + begin

  if (!g_wnd) return;

  // "Show unscribed" checkbox: push a real user change to the setting (the
  // rebuild signature picks the change up on the same frame).
  const bool un = checkbox_get(g_cb_unscribed);
  if (un != g_last_unscribed_cb) {
    g_last_unscribed_cb = un;
    spellbook_settings::set_show_unscribed(un);
  }

  // Filter picker: translate a changed choice into the filter state (the
  // rebuild signature covers the repaint).
  const int fsel = combo_get_cur_choice(g_combo_filter);
  if (fsel != g_last_filter_choice) {
    g_last_filter_choice = fsel;
    if (fsel >= 0 && fsel < static_cast<int>(g_filter_map.size())) {
      g_filter_kind = g_filter_map[fsel].first;
      g_filter_value = g_filter_map[fsel].second;
    }
  }

  if (!is_visible(g_wnd)) return;

  // Stock-book behavior: standing up closes the book. (Grace period after
  // opening covers the round trip of our own /sit.)
  if (g_open_grace > 0) {
    --g_open_grace;
  } else if (player_standstate() == kStandStateStanding) {
    show_window(g_wnd, false);
    return;
  }

  sort_poll();  // header clicks -> our sort state (+ forced repaint)
  if (!g_anchored) {
    try_anchor();     // capture margins + restore persisted geometry once drawn
  } else {
    layout_poll();    // window rect changed -> re-anchor the children
    splitter_poll();  // drag the list/description divider
    geometry_poll();  // persist window/split/column geometry on change
  }

  // Live search text + rebuild signature.
  char *prof = get_pc_profile();
  if (!prof) return;
  const std::string search = to_lower(edit_get_text(g_edit));
  if (search != g_search) g_search = search;
  const uint32_t sig = compute_sig(prof, g_search);
  if (sig != g_last_sig) {
    g_last_sig = sig;
    refresh_list();
  }

  // Memorize progress bar in the status label (fed by the book's own
  // spell-id + countdown fields; restored to the normal text when done).
  if (mem_in_progress()) {
    char *sb = static_cast<char *>(*kSpellBookWnd);
    const bool scribing = *reinterpret_cast<int *>(sb + kBookScribeSlotOffset) != -1;
    const int mem_spell = *reinterpret_cast<int *>(sb + kBookMemSpellIdOffset);  // shared mem/scribe spell id
    const int ticks = *reinterpret_cast<int *>(sb + kBookMemTicksOffset);
    char *sp = spell_by_id(mem_spell);
    char b[96];
    std::snprintf(b, sizeof(b), "%s %s", scribing ? "Scribing" : "Memorizing", sp ? sp + kSpName : "...");
    set_window_text(g_lbl_status, b);
    g_mem_label = true;
    if (++g_mem_log_cd % 30 == 1)
      logger::logf("[book] mem tick: spell=%d ticks=%d active=%d", mem_spell, ticks,
                   (int)*reinterpret_cast<uint8_t *>(sb + kWndActiveFlagOffset));
  } else if (g_mem_label) {
    g_mem_label = false;
    g_mem_log_cd = 0;
    g_last_sig = 0;  // repaint restores the normal status text
  }

  // Click poll: every click updates CurSel AND CurCol, so a change in EITHER is
  // a click (tracking only the row missed gem clicks on the already-selected
  // row). The icon column starts the memorize pickup; any click selects the row
  // and refreshes the description pane. Everything is derived from the row's
  // stored Data (the spell id), never from a side array, so a native reorder
  // can never mis-target. After an icon click the selection is cleared so
  // clicking the same icon again re-fires.
  const int sel = list_get_cur_sel(g_list);
  const int col = *reinterpret_cast<int *>(reinterpret_cast<char *>(g_list) + kListCurColOffset);
  if (sel != g_last_sel || (sel >= 0 && col != g_last_col)) {
    g_last_sel = sel;
    g_last_col = col;
    if (sel >= 0 && sel < list_row_count(g_list)) {
      const int spell_id = static_cast<int>(list_get_item_data(g_list, sel));
      g_sel_spell = spell_id;
      refresh_description();
      if (col == kColGem) {
        try_pickup(spell_id);  // validates scribed/cursor/mem itself, with chat feedback
        list_set_cur_sel(g_list, -1);
        g_last_sel = -1;
      }
    }
  }
}

SpellBookUI::SpellBookUI(RcpService *rcp) {
  load_settings();
  rcp->commands_hook->Add(
      "/rcpbook", {"/rcpsb"},
      "Spell book window: '/rcpbook' opens/closes the new spell-list window. 'on'/'off' = replace the stock "
      "spell book everywhere (also a Display-tab checkbox in /rcpoptions), 'status', and debug: 'dump', "
      "'findwnd', 'gems', 'pickup <spellid>'.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1") {
            spellbook_settings::set_enabled(true);
            print_status();
          } else if (a == "off" || a == "0") {
            spellbook_settings::set_enabled(false);
            print_status();
          } else if (a == "status") {
            print_status();
          } else if (a == "dump") {
            debug_dump();
          } else if (a == "findwnd") {
            debug_findwnd();
          } else if (a == "gems") {
            debug_gems();
          } else if (a == "pickup" && args.size() >= 3) {
            try_pickup(std::atoi(args[2].c_str()));
          } else {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpbook [on|off|status|dump|findwnd|gems|pickup <id>]'");
          }
          return true;
        }
        toggle_window();
        return true;
      });
  rcp->commands_hook->Add(
      "/rcpscribe", {},
      "Right-click to scribe: '/rcpscribe' toggles; 'on|off'. When on, right-click a spell scroll in a bag "
      "to scribe it into the first free spell book slot - the spell book opens (new window or stock, "
      "whichever is active) and standing up or closing it cancels, exactly like scribing by hand.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1")
            spellbook_scribe::set_enabled(true);
          else if (a == "off" || a == "0")
            spellbook_scribe::set_enabled(false);
          else {
            Rcp::Game::print_chat("rof2ClientPlus: '/rcpscribe on|off'");
            return true;
          }
        } else {
          spellbook_scribe::set_enabled(!g_scribe_enabled);
        }
        Rcp::Game::print_chat("rof2ClientPlus right-click-to-scribe: %s", g_scribe_enabled ? "ON" : "OFF");
        return true;
      });
  logger::logf("[book] SpellBookUI constructed; /rcpbook + /rcpscribe registered (enabled=%d unscribed=%d scribe=%d)",
               (int)g_enabled, (int)g_show_unscribed, (int)g_scribe_enabled);
}
