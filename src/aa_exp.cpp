// rof2ClientPlus - automatic AA-experience gating. See aa_exp.h.
#include "aa_exp.h"
#include "rebase.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"   // rcp_guard::run, crash_handler::shutting_down
#include "game_functions.h"  // Rcp::Game::is_in_game / print_chat
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// ---- Game levers (stock RoF2 eqgame.exe, build 2013-05-10; all disasm-verified) ----
// pLocalPC (eqlib pinstLocalPC). The exp/AA data fields are read at pLocalPC + offset
// DIRECTLY - NOT via the +0x2DC8 CharacterZoneClient base, which is only the `this` for
// the Cur_/Max_ vtable accessors (see chat_shortcuts.cpp). Confirmed against four
// independent read/write sites (the AA-window +/- handlers, the server setter at
// 0x4B7060, the exp/aa getters, and the gauge/label builders).
void **const kLocalPC = reinterpret_cast<void **>(::Rcp::eqva(0xDD261C));
constexpr int kOffPercentToAA = 0x2081;  // uint8, 0..100: PercentEXPtoAA (== the AA-window slider value).
constexpr int kOffExp = 0x2910;          // int64: experience WITHIN the current level, range 0..330.
constexpr long long kExpPerLevel = 330;  // Exp == 330 is a full bar (100% into the level). (client const 100/330.)

// The client's own "send AA %" routine: void __cdecl(void). It reads pLocalPC+0x2081, builds the
// 18-byte AA-percent packet (logical opcode 0x424e), and sends it on UdpConnection channel 4; it
// bails internally when the world object (*0xDD25AC) is null. This is exactly what CAAWnd's +/-
// buttons call after stepping the field, so writing the byte then calling this makes the server
// redistribute gained XP just like moving the native AA slider. (eqgame.exe 0x607C40.)
typedef void(__cdecl *SendAaPctFn)();
const uintptr_t kSendAaPct = ::Rcp::eqva(0x607C40);

// CAAWnd::Update (__thiscall) - repaint the AA window's gauge if it happens to be open. Purely
// cosmetic (the packet has already gone out); guarded on a non-null window instance. pinstCAAWnd.
void **const kAAWnd = reinterpret_cast<void **>(::Rcp::eqva(0xD1FC20));
typedef void(__thiscall *AAWndUpdateFn)(void *);
const uintptr_t kAAWndUpdate = ::Rcp::eqva(0x609F90);

// ---- Settings (rof2ClientPlus.ini [Experience]) ----
constexpr char kIniSection[] = "Experience";
constexpr char kIniKeyEnabled[] = "AutoAAExp";
constexpr char kIniKeyThreshold[] = "AutoAAExpThreshold";
constexpr char kIniKeyActive[] = "AutoAAExpActive";

bool g_enabled = false;  // Off by default (opt-in).
int g_threshold = 50;    // Percent into the current level at/above which AA turns on.
int g_active_pct = 100;  // AA % to apply when active (snapped to multiples of 10 = native granularity).

constexpr DWORD kCheckGapMs = 1000;  // XP accrues slowly; a 1s enforcement cadence is ample.
DWORD g_last_check = 0;               // 0 => evaluate on the next tick (see apply_now()).

int clamp_pct(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
int snap10(int v) { return (clamp_pct(v) + 5) / 10 * 10; }  // nearest 10; the native AA granularity.

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKeyEnabled)) g_enabled = ini.getValue<bool>(kIniSection, kIniKeyEnabled);
  if (ini.exists(kIniSection, kIniKeyThreshold)) g_threshold = clamp_pct(ini.getValue<int>(kIniSection, kIniKeyThreshold));
  if (ini.exists(kIniSection, kIniKeyActive)) g_active_pct = snap10(ini.getValue<int>(kIniSection, kIniKeyActive));
}

char *local_pc() { return *reinterpret_cast<char **>(kLocalPC); }

// Progress into the current level as 0..100, or -1 if unreadable. Exp is within-level 0..330.
int read_level_pct(char *pc) {
  if (!pc) return -1;
  long long exp = *reinterpret_cast<long long *>(pc + kOffExp);
  if (exp < 0) exp = 0;
  if (exp > kExpPerLevel) exp = kExpPerLevel;
  return static_cast<int>((exp * 100) / kExpPerLevel);
}

int read_aa_pct(char *pc) {
  if (!pc) return -1;
  return *reinterpret_cast<unsigned char *>(pc + kOffPercentToAA);
}

// Write the AA % and push it to the server the way the AA window does.
void write_aa_pct(char *pc, int pct) {
  pct = clamp_pct(pct);
  *reinterpret_cast<unsigned char *>(pc + kOffPercentToAA) = static_cast<unsigned char>(pct);
  reinterpret_cast<SendAaPctFn>(kSendAaPct)();
  if (void *aawnd = *reinterpret_cast<void **>(kAAWnd)) reinterpret_cast<AAWndUpdateFn>(kAAWndUpdate)(aawnd);
}

}  // namespace

namespace aa_exp {

void apply_now() { g_last_check = 0; }

void on_frame() {
  if (!g_enabled || crash_handler::shutting_down()) return;
  const DWORD now = GetTickCount();
  if (g_last_check && (now - g_last_check) < kCheckGapMs) return;
  g_last_check = now;
  if (!Rcp::Game::is_in_game()) return;

  rcp_guard::run("aa_exp.enforce", [] {
    char *pc = local_pc();
    if (!pc) return;
    const int level_pct = read_level_pct(pc);
    if (level_pct < 0) return;
    const int cur = read_aa_pct(pc);
    const int target = (level_pct >= g_threshold) ? g_active_pct : 0;
    if (cur != target) {
      write_aa_pct(pc, target);
      logger::logf("[aaexp] level XP %d%% %s threshold %d%% -> AA %d%% (was %d%%)", level_pct,
                   level_pct >= g_threshold ? ">=" : "<", g_threshold, target, cur);
    }
  });
}

}  // namespace aa_exp

namespace aa_exp_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  IO_ini(IO_ini::kRcpIniFilename).setValue<bool>(kIniSection, kIniKeyEnabled, g_enabled);
  aa_exp::apply_now();
}
int get_threshold() { return g_threshold; }
void set_threshold(int pct) {
  g_threshold = clamp_pct(pct);
  IO_ini(IO_ini::kRcpIniFilename).setValue<int>(kIniSection, kIniKeyThreshold, g_threshold);
  aa_exp::apply_now();
}
int get_active_pct() { return g_active_pct; }
void set_active_pct(int pct) {
  g_active_pct = snap10(pct);
  IO_ini(IO_ini::kRcpIniFilename).setValue<int>(kIniSection, kIniKeyActive, g_active_pct);
  aa_exp::apply_now();
}
int current_level_pct() {
  if (!Rcp::Game::is_in_game()) return -1;
  int v = -1;
  rcp_guard::run("aa_exp.read_level", [&] { v = read_level_pct(local_pc()); });
  return v;
}
int current_aa_pct() {
  if (!Rcp::Game::is_in_game()) return -1;
  int v = -1;
  rcp_guard::run("aa_exp.read_aa", [&] { v = read_aa_pct(local_pc()); });
  return v;
}
}  // namespace aa_exp_settings

AaExp::AaExp(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  logger::logf("[aaexp] loaded: enabled=%d threshold=%d%% active=%d%%", (int)g_enabled, g_threshold, g_active_pct);

  rcp->commands_hook->Add(
      "/rcpaaexp", {},
      "Automatic AA experience: gate AA XP by how far into the current level you are. '/rcpaaexp on|off', "
      "'/rcpaaexp threshold <0-100>' (level XP% to switch AA on), '/rcpaaexp active <0-100>' (AA% when on).",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "on" || a == "1")
            aa_exp_settings::set_enabled(true);
          else if (a == "off" || a == "0")
            aa_exp_settings::set_enabled(false);
          else if ((a == "threshold" || a == "thresh") && args.size() >= 3)
            aa_exp_settings::set_threshold(std::atoi(args[2].c_str()));
          else if ((a == "active" || a == "amount") && args.size() >= 3)
            aa_exp_settings::set_active_pct(std::atoi(args[2].c_str()));
          else
            aa_exp_settings::set_enabled(!aa_exp_settings::get_enabled());
        }
        const int lvl = aa_exp_settings::current_level_pct();
        const int aa = aa_exp_settings::current_aa_pct();
        const std::string lvls = lvl < 0 ? "n/a" : std::to_string(lvl) + "%";
        const std::string aas = aa < 0 ? "n/a" : std::to_string(aa) + "%";
        Rcp::Game::print_chat(
            "rof2ClientPlus auto-AA: %s. Turn AA on at level XP >= %d%%, use %d%% AA when on. (now: level XP %s, AA %s)",
            aa_exp_settings::get_enabled() ? "ON" : "off", aa_exp_settings::get_threshold(),
            aa_exp_settings::get_active_pct(), lvls.c_str(), aas.c_str());
        return true;
      });
  logger::log("[aaexp] /rcpaaexp registered");
}
