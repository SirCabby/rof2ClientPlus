// rof2ClientPlus - Zeal-style chat token shortcuts. See chat_shortcuts.h.
#include "chat_shortcuts.h"
#include "rebase.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "crash_handler.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "logger.h"
#include "rcp.h"

namespace {

// CEverQuest::DoPercentConvert(char* line, bool bOutGoing) - __thiscall, this = the game object.
// The stock client calls this on every outgoing chat line to expand %t/%s/... tokens; we detour
// it to expand our own tokens first. Address confirmed against the 2013-05-10 binary and eqlib
// (CEverQuest__DoPercentConvert_x = 0x51B600); the 0x538110 that game_functions.h/game_structures.h
// used to carry is a stale TAKP offset (that address is mid-function in RoF2).
const uintptr_t kDoPercentConvert = ::Rcp::eqva(0x51B600);
typedef void(__fastcall *DoPercentConvertFn)(void *game, int edx, char *line, int b_outgoing);
DoPercentConvertFn g_orig = nullptr;

// Self/target entity pointers - the eqlib-confirmed RoF2 globals (pinstLocalPlayer / pinstTarget),
// the same ones target_ring.cpp / nameplate.cpp read directly. NOTE: do NOT use Rcp::Game::get_self()
// / get_target() here - those go through the STALE TAKP game_addresses.h globals (Self 0x7F94CC,
// Target 0x7F94EC) and return null on this client (that was the "%loc/%thp read 0" bug).
void **const kSelfPtr = reinterpret_cast<void **>(::Rcp::eqva(0xDD2630));    // pinstLocalPlayer
void **const kTargetPtr = reinterpret_cast<void **>(::Rcp::eqva(0xDD2648));  // pinstTarget

// Local player character-stat accessors (same mechanism the billboard nameplates use in
// font_overlay.cpp, proven in-game). pLocalPC + 0x2DC8 is the CharacterZoneClient the client's own
// Cur_/Max_ accessor functions expect as `this`.
void **const kLocalPC = reinterpret_cast<void **>(::Rcp::eqva(0xDD261C));
constexpr int kCzcOffset = 0x2DC8;
typedef int(__thiscall *StatFn2)(void *, int, bool);  // Cur_HP(0x449E00), Max_HP(0x443FA0): (0, true)
typedef int(__thiscall *StatFn1b)(void *, bool);      // Cur_Mana(0x4442E0): (true)
typedef int(__thiscall *StatFn1i)(void *, int);       // Max_Mana(0x581E60): (0)
const uintptr_t kCurHP = ::Rcp::eqva(0x449E00), kMaxHP = ::Rcp::eqva(0x443FA0), kCurMana = ::Rcp::eqva(0x4442E0), kMaxMana = ::Rcp::eqva(0x581E60);

// Entity field offsets (disasm-confirmed; shared with nameplate.cpp / target_ring.cpp).
constexpr int kEntY = 0x64, kEntX = 0x68, kEntZ = 0x6c;  // position floats (EQ order Y, X, Z)
constexpr int kEntHPMax = 0x2dc, kEntHPCurrent = 0x2e4;  // int32; NPCs carry a bare percent in Current (max 0)

int clamp_pct(long long v) { return v < 0 ? 0 : (v > 100 ? 100 : static_cast<int>(v)); }

// Percent from a cur/max pair; when max <= 0 (NPC targets) treat cur as an already-computed percent.
int percent_of(int cur, int mx) { return mx > 0 ? clamp_pct((long long)cur * 100 / mx) : clamp_pct(cur); }

// Self HP/Mana percent via the client's own accessor functions, or -1 if unavailable.
int self_hp_percent() {
  char *pc = *reinterpret_cast<char **>(kLocalPC);
  if (!pc) return -1;
  void *czc = pc + kCzcOffset;
  const int cur = reinterpret_cast<StatFn2>(kCurHP)(czc, 0, true);
  const int mx = reinterpret_cast<StatFn2>(kMaxHP)(czc, 0, true);
  return mx > 0 ? percent_of(cur, mx) : -1;
}

int self_mana_percent() {
  char *pc = *reinterpret_cast<char **>(kLocalPC);
  if (!pc) return -1;
  void *czc = pc + kCzcOffset;
  const int cur = reinterpret_cast<StatFn1b>(kCurMana)(czc, true);
  const int mx = reinterpret_cast<StatFn1i>(kMaxMana)(czc, 0);
  return mx > 0 ? percent_of(cur, mx) : -1;
}

// A percent value formatted like the UI ("73%"); a missing value (no mana pool, no target) reads 0%.
std::string pct_str(int pct) { return std::to_string(pct < 0 ? 0 : pct) + "%"; }

// Plain literal replace-all (no regex): the tokens contain no metacharacters, this is the chat
// hot path, and no replacement value can introduce another token, so a single left-to-right pass
// per token is sufficient and order between the four tokens is irrelevant.
void replace_all(std::string &s, const char *from, const std::string &to) {
  const size_t flen = std::strlen(from);
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, flen, to);
    pos += to.size();
  }
}

// The detour. Only outgoing chat (b_outgoing) with a '%' present, and only while in game, gets our
// substitution; everything else falls straight through to the client. The client guarantees the
// `line` buffer is at least its own 0x800-byte working size (DoPercentConvert expands %t etc. into
// it), so a bounded write below that is safe for every caller.
void __fastcall DoPercentConvert_hk(void *game, int edx, char *line, int b_outgoing) {
  if (line && b_outgoing && line[0] && std::strchr(line, '%') && Rcp::Game::is_in_game()) {
    // Read the (game-memory) values under the crash guard, then build the replacement string
    // afterwards - std::string ops touch only the heap, so they can't fault inside the guard.
    int hp = -1, mana = -1, thp = -1;
    bool have_loc = false;
    float ly = 0.f, lx = 0.f, lz = 0.f;
    rcp_guard::run("chat_shortcuts.read", [&] {
      hp = self_hp_percent();
      mana = self_mana_percent();
      if (char *self = *reinterpret_cast<char **>(kSelfPtr)) {
        ly = *reinterpret_cast<float *>(self + kEntY);
        lx = *reinterpret_cast<float *>(self + kEntX);
        lz = *reinterpret_cast<float *>(self + kEntZ);
        have_loc = true;
      }
      if (char *tgt = *reinterpret_cast<char **>(kTargetPtr)) {
        thp = percent_of(*reinterpret_cast<int *>(tgt + kEntHPCurrent), *reinterpret_cast<int *>(tgt + kEntHPMax));
      }
    });

    std::string s = line;
    if (s.find("%loc") != std::string::npos) {
      char loc[64];
      if (have_loc)
        std::snprintf(loc, sizeof(loc), "%.2f, %.2f, %.2f", ly, lx, lz);
      else
        std::strcpy(loc, "0.00, 0.00, 0.00");
      replace_all(s, "%loc", loc);
    }
    if (s.find("%thp") != std::string::npos) replace_all(s, "%thp", pct_str(thp));
    if (s.find("%n") != std::string::npos) replace_all(s, "%n", pct_str(mana));
    if (s.find("%h") != std::string::npos) replace_all(s, "%h", pct_str(hp));

    // Copy back, bounded well under the client's 0x800 working buffer.
    constexpr size_t kMaxLine = 2000;
    const size_t n = std::min(s.size(), kMaxLine);
    std::memcpy(line, s.c_str(), n);
    line[n] = '\0';
  }
  g_orig(game, edx, line, b_outgoing);
}

}  // namespace

ChatShortcuts::ChatShortcuts(RcpService *rcp) {
  rcp->hooks->Add("chat_shortcuts", static_cast<int>(kDoPercentConvert), DoPercentConvert_hk, hook_type_detour);
  g_orig = rcp->hooks->hook_map["chat_shortcuts"]->original(DoPercentConvert_hk);
  logger::logf("[chat] DoPercentConvert detour installed at 0x%x (%%n/%%h/%%loc/%%thp always on)",
               (unsigned)kDoPercentConvert);
}

ChatShortcuts::~ChatShortcuts() {}
