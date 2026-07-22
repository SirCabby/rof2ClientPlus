// rof2ClientPlus - windowed-startup diagnostics + opt-in self-heal. See window_watch.h.
#include "window_watch.h"

#include <windows.h>
// tlhelp32 must follow windows.h.
#include <tlhelp32.h>

#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

#include "commands.h"
#include "crash_handler.h"
#include "directx.h"
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "rcp.h"

namespace {

// ---- Live settings (persisted to rof2ClientPlus.ini [Window]). ----
bool g_self_heal = false;  // Force the window visible/foreground on a broken windowed start. Opt-in.
bool g_verbose = false;    // Log the chatty window messages (pos-changing, focus) too.
bool g_guard = true;       // Wrap our detour bodies in the fault net (crash containment). Default on.
bool g_char_title = true;  // Put the logged-in character's name in the window title. On by default (see sync_title).
bool g_raise = true;   // Multibox raise-on-activate: keep wineserver z-order tracking compositor activation. THE fix.
bool g_dedup = false;  // Multibox move layers (spawn shift + runtime move): opt-in fallback only - the runtime move
                       // fought rects the user arranged on purpose (maximize onto the sibling's monitor = "collision").

constexpr char kIni[] = "Window";
constexpr DWORD kHealWindowMs = 8000;  // Only self-heal during the window's first few seconds.
constexpr DWORD kHealMinGapMs = 250;   // Don't fight the client every frame.
constexpr DWORD kHealthDenseMs = 20000;  // Dense per-frame health trace for the first ~20s.
constexpr DWORD kHealthGapMs = 500;
constexpr DWORD kDedupWindowMs = 30000;  // Auto-dedup only during our first ~30s (the spawn collision); never fight the user later.
constexpr DWORD kDedupGapMs = 2000;      // Re-check cadence inside that window (also lets simultaneous launches converge).
constexpr int kDedupEdgeTolPx = 16;      // Rects within this per-edge = "the same rect" (spawn collisions are byte-identical).

// ---- Runtime state. ----
HWND g_hwnd = nullptr;
WNDPROC g_orig_wndproc = nullptr;
std::string g_orig_title;     // The client's own window caption, captured at subclass; restored when not in-world.
std::string g_pending_title;  // Title the wndproc should apply (see WM_SETTITLE handling); render-thread-only, no lock.
std::string g_posted_title;   // Last title we asked for, so on_frame only re-posts on an actual change.
UINT g_wm_settitle = 0;       // RegisterWindowMessage id: "apply g_pending_title" - handled in wndproc, off the ProcessGameEvents stack.
UINT g_wm_dedup = 0;          // RegisterWindowMessage id: "apply the pending dedup move" - same pattern as g_wm_settitle.
RECT g_dedup_target = {};     // Normal-position rect for the pending move; render-thread-only, no lock.
bool g_dedup_maximize = false;  // Re-maximize (onto the target's monitor) after the move.
int g_dedup_attempts = 0;       // Posted moves this session; caps the runtime loop (KWin can revert our move).
bool g_dedup_gave_up = false;   // Runtime dedup exhausted its attempts; logged once.
bool g_spawn_checked = false;   // CreateWindowExA hook: the main window was already seen (one-shot).
int g_spawn_seen = 0;           // Top-level creations logged so far (learning instrument; first few only).
DWORD g_first_seen = 0;
DWORD g_last_heal = 0;
DWORD g_last_dedup = 0;
DWORD g_dedup_first_delay = 0;  // Per-process jitter so simultaneously-launched boxes don't decide at the same instant.
DWORD g_last_health = 0;
unsigned long g_loop = 0;  // Main-loop iteration count (proxy for "the client is alive").
int g_last_vis = -1, g_last_icon = -1, g_last_fg = -1;

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIni, "SelfHeal")) g_self_heal = ini.getValue<bool>(kIni, "SelfHeal");
  if (ini.exists(kIni, "VerboseLog")) g_verbose = ini.getValue<bool>(kIni, "VerboseLog");
  if (ini.exists(kIni, "GuardDetours")) g_guard = ini.getValue<bool>(kIni, "GuardDetours");
  if (ini.exists(kIni, "CharTitle")) g_char_title = ini.getValue<bool>(kIni, "CharTitle");
  if (ini.exists(kIni, "MultiboxRaise")) g_raise = ini.getValue<bool>(kIni, "MultiboxRaise");
  // Deliberately a NEW key (old "MultiboxDedup" inis had TRUE persisted; the move layers
  // are demoted to opt-in now that raise-on-activate is the real fix - stale key ignored).
  if (ini.exists(kIni, "MultiboxMove")) g_dedup = ini.getValue<bool>(kIni, "MultiboxMove");
  rcp_guard::set_enabled(g_guard);
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIni, "SelfHeal", g_self_heal);
  ini.setValue<bool>(kIni, "VerboseLog", g_verbose);
  ini.setValue<bool>(kIni, "GuardDetours", g_guard);
  ini.setValue<bool>(kIni, "CharTitle", g_char_title);
  ini.setValue<bool>(kIni, "MultiboxRaise", g_raise);
  ini.setValue<bool>(kIni, "MultiboxMove", g_dedup);
}

bool belongs_to_us(HWND h) {
  if (!h) return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(h, &pid);
  return pid == GetCurrentProcessId();
}

// The work area of the monitor the window is (mostly) on.
RECT monitor_work(HWND h) {
  HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (mon && GetMonitorInfoA(mon, &mi)) return mi.rcWork;
  RECT r = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
  return r;
}

// Nudge the window fully onto its monitor's work area (move only, never resize, so we
// never fight the client's chosen size). Returns true if it had to move it.
bool clamp_onscreen(HWND h) {
  RECT wr;
  if (!GetWindowRect(h, &wr)) return false;
  const RECT a = monitor_work(h);
  const int w = wr.right - wr.left, ht = wr.bottom - wr.top;
  int x = wr.left, y = wr.top;
  if (x + w > a.right) x = a.right - w;
  if (y + ht > a.bottom) y = a.bottom - ht;
  if (x < a.left) x = a.left;
  if (y < a.top) y = a.top;
  if (x == wr.left && y == wr.top) return false;
  SetWindowPos(h, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  logger::logf("[win] clamp on-screen: (%ld,%ld) -> (%d,%d) [work %ld,%ld-%ld,%ld]", wr.left, wr.top, x, y, a.left,
               a.top, a.right, a.bottom);
  return true;
}

// Forces the window visible, restored, on-screen, and foreground - the corrective pass.
void heal(HWND h, bool vis, bool icon, bool fg) {
  clamp_onscreen(h);
  if (icon || !vis) {
    ShowWindow(h, SW_RESTORE);
    ShowWindow(h, SW_SHOW);
  }
  BringWindowToTop(h);
  SetForegroundWindow(h);
  logger::logf("[win] self-heal applied (was vis=%d icon=%d fg=%d)", (int)vis, (int)icon, (int)fg);
}

std::string window_title(HWND h) {
  char buf[128] = {0};
  GetWindowTextA(h, buf, sizeof(buf));
  return buf;
}

// ---- Find the game's main top-level window owned by our process. get_game_window()
// needs eqw.dll, which this build doesn't ship, so we enumerate instead and pick the
// largest top-level (captioned or visible) belonging to us. ----
struct FindCtx {
  HWND best;
  long best_area;
};

BOOL CALLBACK enum_find(HWND h, LPARAM lp) {
  FindCtx *ctx = reinterpret_cast<FindCtx *>(lp);
  if (!belongs_to_us(h)) return TRUE;
  if (GetWindow(h, GA_ROOT) != h) return TRUE;  // Top-level only.
  const LONG style = GetWindowLongA(h, GWL_STYLE);
  const bool candidate = (style & WS_CAPTION) || IsWindowVisible(h);
  if (!candidate) return TRUE;
  RECT r;
  if (!GetWindowRect(h, &r)) return TRUE;
  const long area = (long)(r.right - r.left) * (long)(r.bottom - r.top);
  if (area > ctx->best_area) {
    ctx->best_area = area;
    ctx->best = h;
  }
  return TRUE;
}

HWND find_main_window() {
  FindCtx ctx = {nullptr, 0};
  EnumWindows(enum_find, reinterpret_cast<LPARAM>(&ctx));
  return ctx.best;
}

// ---- Multibox input routing.
//
// All boxed clients live in ONE wineserver session, and wineserver routes mouse input
// by hit-testing the cursor against ITS window rects in ITS z-order - state that
// KWin's visual stacking and focus never feed back into wine. Verified live
// 2026-07-21: both clients held rect=(2556,5)-(5124,1404) (shared eqclient.ini spawn)
// while X11 showed them on different monitors, and clicks activated the wrong client.
// Two layers deal with it:
//
// 1) Raise-on-activate (wndproc WM_ACTIVATE, g_raise, default ON) - THE fix. Whenever
//    the compositor activates a client (X FocusIn -> wine foreground -> WM_ACTIVATE),
//    that client raises itself to the top of wineserver's z-order, so input-topmost
//    converges to visually-topmost and deliberately-overlapping clients each get their
//    own clicks. Its partner is the WM_MOUSEACTIVATE mute: a click can race ahead of
//    the newly-activated sibling's raise, get hit-tested against the STALE z-order,
//    land in the old client, and wine's click-activation then steals the focus right
//    back ("flicks back, retry until it sticks"). While a sibling client exists we
//    return MA_NOACTIVATEANDEAT, so clicks never activate wine-side - every click that
//    should activate us also activates us through the compositor a beat later, making
//    KWin the single focus authority.
//
// 2) Rect dedup (g_dedup, default OFF) - the older, blunter layer: spawn-shift (IAT
//    hook below) + runtime move so clients never share a rect at all. Correct routing
//    no longer needs distinct rects, and the runtime move FOUGHT deliberate layouts:
//    maximizing the second client onto the first's monitor looks exactly like a spawn
//    collision, so for the window's first 30s the maximize kept getting reverted. Kept
//    as an opt-in fallback (/rcpwindow dedup on; spawn hook arms on next launch). ----

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

struct SiblingWnd {
  HWND h;
  DWORD pid;
  RECT r;
};

BOOL CALLBACK enum_siblings(HWND h, LPARAM lp) {
  auto *out = reinterpret_cast<std::vector<SiblingWnd> *>(lp);
  char cls[64] = {0};
  GetClassNameA(h, cls, sizeof(cls));
  if (strcmp(cls, "_EverQuestwndclass") != 0) return TRUE;  // The client's wndclass (verified live in this build).
  DWORD pid = 0;
  GetWindowThreadProcessId(h, &pid);
  if (pid == GetCurrentProcessId()) return TRUE;
  if (!IsWindowVisible(h) || IsIconic(h)) return TRUE;  // Minimized windows park at bogus rects.
  RECT r;
  if (!GetWindowRect(h, &r)) return TRUE;
  out->push_back({h, pid, r});
  return TRUE;
}

// Any other visible client window in the session right now? One EnumWindows pass -
// only server-side queries, so it's safe from the wndproc (no message sends).
bool sibling_exists() {
  std::vector<SiblingWnd> sibs;
  EnumWindows(enum_siblings, reinterpret_cast<LPARAM>(&sibs));
  return !sibs.empty();
}

// Process creation time as a sortable value; 0 if unknown (treated as older than us,
// so on doubt WE move - staying collided is the worse failure).
ULONGLONG process_start_time(DWORD pid) {
  ULONGLONG t = 0;
  const bool self = (pid == GetCurrentProcessId());
  HANDLE p = self ? GetCurrentProcess() : OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (p) {
    FILETIME c, e, k, u;
    if (GetProcessTimes(p, &c, &e, &k, &u)) t = (ULONGLONG(c.dwHighDateTime) << 32) | c.dwLowDateTime;
    if (!self) CloseHandle(p);
  }
  return t;
}

bool rects_near_equal(const RECT &a, const RECT &b, int tol) {
  return abs(a.left - b.left) <= tol && abs(a.top - b.top) <= tol && abs(a.right - b.right) <= tol &&
         abs(a.bottom - b.bottom) <= tol;
}

BOOL CALLBACK enum_monitors(HMONITOR mon, HDC, LPRECT, LPARAM lp) {
  auto *out = reinterpret_cast<std::vector<RECT> *>(lp);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoA(mon, &mi)) out->push_back(mi.rcWork);
  return TRUE;
}

// Monitors with no sibling client on them, preferred order: same row as the primary
// (primary sits at y=0 in Windows coords) before other rows, then left to right - so
// box #2 lands beside box #1, not above it.
std::vector<RECT> free_monitors(const std::vector<SiblingWnd> &sibs) {
  std::vector<RECT> mons;
  EnumDisplayMonitors(nullptr, nullptr, enum_monitors, reinterpret_cast<LPARAM>(&mons));
  std::vector<RECT> out;
  for (const auto &m : mons) {
    bool taken = false;
    for (const auto &s : sibs) {
      const POINT c = {(s.r.left + s.r.right) / 2, (s.r.top + s.r.bottom) / 2};
      if (PtInRect(&m, c)) {
        taken = true;
        break;
      }
    }
    if (!taken) out.push_back(m);
  }
  for (size_t i = 0; i + 1 < out.size(); i++)  // Tiny N; simple selection sort.
    for (size_t j = i + 1; j < out.size(); j++)
      if (labs(out[j].top) < labs(out[i].top) || (labs(out[j].top) == labs(out[i].top) && out[j].left < out[i].left)) {
        RECT t = out[i];
        out[i] = out[j];
        out[j] = t;
      }
  return out;
}

// ---- Spawn-position offset (the strongest dedup: prevent the collision at birth).
//
// The runtime move below can be undone by KWin: a maximized window's geometry belongs
// to the compositor, and KWin re-maximizes onto the monitor IT has the window on, so a
// wineserver-side move "sticks" for under 2s (observed live). At creation time we hold
// the real lever: the game passes the shared eqclient.ini position to CreateWindowExA,
// and window managers honor an app's initial placement - so shifting x/y here puts the
// window (and the game's own follow-up maximize) on a free monitor in BOTH worlds.
// Delivered as an IAT patch on eqgame.exe's user32!CreateWindowExA import: no code
// bytes touched, only the game's own calls re-routed. ----

using CreateWindowExA_t = HWND(WINAPI *)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE,
                                         LPVOID);
CreateWindowExA_t g_real_cwexa = nullptr;

HWND WINAPI CreateWindowExA_hk(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h,
                               HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
  const bool cls_is_atom = (reinterpret_cast<ULONG_PTR>(cls) >> 16) == 0;
  const bool top_level = !parent && !(style & WS_CHILD);
  if (top_level && g_spawn_seen < 8) {
    // Learning instrument: only the game's OWN creations flow through the exe IAT
    // (system-made helpers like the IME windows don't), so this is a handful of lines
    // that show exactly what the main-window creation call looks like.
    ++g_spawn_seen;
    if (cls_is_atom)
      logger::logf("[win] dedup: CreateWindowExA cls=#%04x '%s' xy=(%d,%d) wh=(%d,%d) style=0x%lx",
                   (unsigned)reinterpret_cast<ULONG_PTR>(cls), title ? title : "", x, y, w, h, (unsigned long)style);
    else
      logger::logf("[win] dedup: CreateWindowExA cls='%s' '%s' xy=(%d,%d) wh=(%d,%d) style=0x%lx", cls,
                   title ? title : "", x, y, w, h, (unsigned long)style);
  }
  // The main game window: first top-level with the client's class (an ATOM class is
  // accepted too - nothing else top-level comes through this IAT).
  if (g_dedup && !g_spawn_checked && top_level && (cls_is_atom || strstr(cls, "EverQuest"))) {
    g_spawn_checked = true;
    std::vector<SiblingWnd> sibs;
    EnumWindows(enum_siblings, reinterpret_cast<LPARAM>(&sibs));
    if (!sibs.empty()) {
      const std::vector<RECT> free_m = free_monitors(sibs);
      const int ox = x, oy = y;
      if (!free_m.empty()) {
        x = free_m[0].left + 16;
        y = free_m[0].top + 16;
      } else if (x != CW_USEDEFAULT) {  // Every monitor taken: at least make the rect distinct (cascade).
        x += 48 * (int)sibs.size();
        y += 48 * (int)sibs.size();
      }
      logger::logf("[win] dedup: spawn shifted (%d,%d) -> (%d,%d) (%zu sibling client(s)%s)", ox, oy, x, y,
                   sibs.size(), free_m.empty() ? ", no free monitor - cascade" : "");
    } else {
      logger::log("[win] dedup: first client - spawn position untouched");
    }
  }
  return g_real_cwexa(ex, cls, title, style, x, y, w, h, parent, menu, inst, param);
}

// Swap eqgame.exe's user32!CreateWindowExA import pointer (calls from other modules -
// dxvk, dinput proxies, MQ - keep their own resolution and are unaffected).
void install_spawn_hook() {
  BYTE *base = reinterpret_cast<BYTE *>(GetModuleHandleA(nullptr));
  auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
  const IMAGE_DATA_DIRECTORY &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!dir.VirtualAddress) {
    logger::log("[win] dedup: exe has no import directory?! spawn hook not installed");
    return;
  }
  for (auto *imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + dir.VirtualAddress); imp->Name; ++imp) {
    if (_stricmp(reinterpret_cast<const char *>(base + imp->Name), "user32.dll") != 0) continue;
    auto *oft = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->OriginalFirstThunk);
    auto *ft = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->FirstThunk);
    for (; oft->u1.AddressOfData; ++oft, ++ft) {
      if (oft->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
      auto *ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + oft->u1.AddressOfData);
      if (strcmp(reinterpret_cast<const char *>(ibn->Name), "CreateWindowExA") != 0) continue;
      DWORD old = 0;
      VirtualProtect(&ft->u1.Function, sizeof(ft->u1.Function), PAGE_READWRITE, &old);
      g_real_cwexa = reinterpret_cast<CreateWindowExA_t>(ft->u1.Function);
      ft->u1.Function = reinterpret_cast<DWORD>(&CreateWindowExA_hk);
      VirtualProtect(&ft->u1.Function, sizeof(ft->u1.Function), old, &old);
      logger::logf("[win] dedup: spawn hook installed (IAT user32!CreateWindowExA, real=%p)", (void *)g_real_cwexa);
      return;
    }
  }
  logger::log("[win] dedup: CreateWindowExA not found in exe IAT - spawn hook not installed");
}

// One decision pass (cheap; a handful of win32 queries). Runs on the ProcessGameEvents
// stack, so it never touches the window itself - it posts g_wm_dedup and wndproc_hk
// does the surgery (same constraint as sync_title; see that comment).
void dedup_pass(bool manual) {
  if (!g_hwnd) return;
  RECT mine;
  if (!GetWindowRect(g_hwnd, &mine)) return;

  std::vector<SiblingWnd> sibs;
  EnumWindows(enum_siblings, reinterpret_cast<LPARAM>(&sibs));

  std::vector<const SiblingWnd *> colliders;
  for (const auto &s : sibs)
    if (rects_near_equal(mine, s.r, kDedupEdgeTolPx)) colliders.push_back(&s);
  if (colliders.empty()) {
    g_dedup_attempts = 0;  // Clean state: a later collision starts a fresh attempt budget.
    g_dedup_gave_up = false;
    if (manual) logger::logf("[win] dedup: no collision (%zu sibling client window(s))", sibs.size());
    return;
  }

  // Only the newest process of the colliding set moves - the older client is where the
  // user is already playing. Each client decides independently with the same rule, so
  // exactly the right ones move even on simultaneous launches.
  const ULONGLONG my_start = process_start_time(GetCurrentProcessId());
  const DWORD my_pid = GetCurrentProcessId();
  bool i_move = false;
  for (const SiblingWnd *c : colliders) {
    const ULONGLONG their_start = process_start_time(c->pid);
    if (their_start < my_start || (their_start == my_start && c->pid < my_pid)) {
      i_move = true;
      break;
    }
  }
  if (!i_move) {
    logger::logf("[win] dedup: rect collides with %zu newer sibling(s) - they move, we stay", colliders.size());
    return;
  }

  // The runtime move is best-effort: KWin owns a maximized window's geometry and can
  // snap our rect right back (observed live - each move held <2s). Two attempts at the
  // clean fix (maximize onto a free monitor), two at the fallback (un-maximized
  // cascade: a restored window's rect the compositor has no reason to re-assert), then
  // stop rather than flap. The spawn hook is the reliable fix on the next launch.
  if (g_dedup_attempts >= 4) {
    if (!g_dedup_gave_up) {
      g_dedup_gave_up = true;
      logger::log(
          "[win] dedup: giving up - the compositor keeps re-asserting the colliding rect. Un-maximize or drag "
          "this client, or relaunch (the spawn hook then separates the clients at creation).");
    }
    return;
  }

  int rank = 0;  // How many siblings are older than us - used to spread cascade slots.
  for (const auto &s : sibs) {
    const ULONGLONG ts = process_start_time(s.pid);
    if (ts < my_start || (ts == my_start && s.pid < my_pid)) ++rank;
  }
  const std::vector<RECT> free_m = free_monitors(sibs);
  const bool try_monitor = g_dedup_attempts < 2 && !free_m.empty();
  if (try_monitor) {
    // Park the normal rect inside the free monitor (margin keeps it clearly inside);
    // the wndproc re-maximizes there if we were maximized.
    const RECT &m = free_m[0];
    g_dedup_target = {m.left + 32, m.top + 32, m.right - 32, m.bottom - 32};
    g_dedup_maximize = IsZoomed(g_hwnd);
  } else {
    const RECT a = monitor_work(g_hwnd);
    const int off = 48 * (rank > 0 ? rank : 1);
    const int w = (a.right - a.left) * 9 / 10, h = (a.bottom - a.top) * 9 / 10;
    g_dedup_target = {a.left + off, a.top + off, a.left + off + w, a.top + off + h};
    if (g_dedup_target.right > a.right) OffsetRect(&g_dedup_target, a.right - g_dedup_target.right, 0);
    if (g_dedup_target.bottom > a.bottom) OffsetRect(&g_dedup_target, 0, a.bottom - g_dedup_target.bottom);
    g_dedup_maximize = false;  // Same monitor as the collider: staying maximized would recreate the collision.
  }
  ++g_dedup_attempts;
  logger::logf(
      "[win] dedup: rect (%ld,%ld)-(%ld,%ld) collides with %zu sibling(s); attempt %d: moving to (%ld,%ld)-(%ld,%ld)%s%s",
      mine.left, mine.top, mine.right, mine.bottom, colliders.size(), g_dedup_attempts, g_dedup_target.left,
      g_dedup_target.top, g_dedup_target.right, g_dedup_target.bottom, g_dedup_maximize ? " +maximize" : "",
      try_monitor ? "" : " (cascade)");
  if (!g_wm_dedup) g_wm_dedup = RegisterWindowMessageA("RofClientPlus_DedupMove");
  PostMessageA(g_hwnd, g_wm_dedup, 0, 0);  // Non-blocking; applied in wndproc_hk.
}

// ---- Subclass: log the lifecycle messages that reveal WHY the window vanishes. ----
LRESULT CALLBACK wndproc_hk(HWND h, UINT msg, WPARAM w, LPARAM l) {
  // Apply the character-name title here, NOT from sync_title: this fires during the
  // game's normal message dispatch (top of the loop), so SetWindowTextA is safe - unlike
  // calling it from inside ProcessGameEvents, which deadlocked under wine. See sync_title.
  if (msg == g_wm_settitle && g_wm_settitle) {
    SetWindowTextA(h, g_pending_title.c_str());
    logger::logf("[win] window title -> '%s'", g_pending_title.c_str());
    return 0;
  }
  if (msg == g_wm_dedup && g_wm_dedup) {
    // The classic cross-monitor move: restore -> place the normal rect on the target
    // -> re-maximize there. Runs at the top of the game's message loop (safe context).
    ShowWindow(h, SW_RESTORE);
    SetWindowPos(h, nullptr, g_dedup_target.left, g_dedup_target.top, g_dedup_target.right - g_dedup_target.left,
                 g_dedup_target.bottom - g_dedup_target.top, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_dedup_maximize) ShowWindow(h, SW_MAXIMIZE);
    RECT after = {};
    GetWindowRect(h, &after);
    logger::logf("[win] dedup: moved; wineserver rect now (%ld,%ld)-(%ld,%ld)%s", after.left, after.top, after.right,
                 after.bottom, g_dedup_maximize ? " (maximized)" : "");
    return 0;
  }
  switch (msg) {
    case WM_ACTIVATEAPP:
      logger::logf("[win] WM_ACTIVATEAPP active=%d", (int)w);
      break;
    case WM_MOUSEACTIVATE:
      // Multibox: mute wine-side click activation (see "Multibox input routing" above).
      // A click can be hit-tested against wineserver's STALE z-order - racing the
      // just-activated sibling's raise - land in the wrong client, and click-activate
      // it, stealing back the focus the compositor just gave the other client. Eating
      // the click also keeps it from acting inside the wrong client's world. Every
      // click that SHOULD activate us does so via KWin (FocusIn -> WM_ACTIVATE below)
      // a beat later; with no sibling running, stock behavior.
      if (g_raise && sibling_exists()) {
        logger::log("[win] WM_MOUSEACTIVATE -> eaten (multibox: activation follows the compositor)");
        return MA_NOACTIVATEANDEAT;
      }
      break;
    case WM_ACTIVATE:
      // Multibox: mirror the activation into wineserver's z-order. KWin raises the
      // window it activates VISUALLY, but nothing raises it on the wine side - and
      // wineserver routes mouse input by hit-testing ITS z-order, so with overlapping
      // clients every click in the shared region went to the stale-topmost ("original")
      // client forever, and alt-tab/taskbar switching couldn't fix it. Raising here
      // keeps input-topmost == visually-topmost after any activation, exactly the
      // native Windows contract (activated window comes to top). NOACTIVATE avoids
      // re-entry; the losing client only sees INACTIVE and does nothing.
      if (LOWORD(w) != WA_INACTIVE && g_raise)
        SetWindowPos(h, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      logger::logf("[win] WM_ACTIVATE %s%s", LOWORD(w) == WA_INACTIVE ? "INACTIVE" : "ACTIVE",
                   (LOWORD(w) != WA_INACTIVE && g_raise) ? " (raised to wineserver top)" : "");
      break;
    case WM_KILLFOCUS:
      logger::log("[win] WM_KILLFOCUS");
      break;
    case WM_SETFOCUS:
      if (g_verbose) logger::log("[win] WM_SETFOCUS");
      break;
    case WM_SHOWWINDOW:
      logger::logf("[win] WM_SHOWWINDOW shown=%d reason=%d", (int)w, (int)l);
      break;
    case WM_SIZE:
      logger::logf("[win] WM_SIZE %s %dx%d",
                   w == SIZE_MINIMIZED ? "MINIMIZED" : w == SIZE_MAXIMIZED ? "MAXIMIZED" : "restored", LOWORD(l),
                   HIWORD(l));
      break;
    case WM_WINDOWPOSCHANGING: {
      WINDOWPOS *p = reinterpret_cast<WINDOWPOS *>(l);
      if (g_verbose || (p->flags & (SWP_HIDEWINDOW | SWP_SHOWWINDOW)))
        logger::logf("[win] WM_WINDOWPOSCHANGING flags=0x%x xy=(%d,%d) wh=(%d,%d)", p->flags, p->x, p->y, p->cx, p->cy);
      break;
    }
    case WM_CLOSE:
      logger::log("[win] WM_CLOSE");
      crash_handler::begin_shutdown();  // Quiesce our detours + hand teardown back to stock (graceful /exit).
      break;
    case WM_DESTROY:
      logger::log("[win] WM_DESTROY");
      crash_handler::begin_shutdown();  // (Also covers /exit, which reaches teardown without a WM_CLOSE.)
      break;
    default:
      break;
  }
  return CallWindowProcA(g_orig_wndproc, h, msg, w, l);
}

void subclass(HWND h) {
  g_orig_wndproc = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrA(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc_hk)));
  g_orig_title = window_title(h);  // The stock caption, restored whenever we're not in-world.
  logger::logf("[win] subclassed main window %p title='%s' orig_wndproc=%p", (void *)h, g_orig_title.c_str(),
               (void *)g_orig_wndproc);
}

// ---- Reflect the logged-in character in the OS window title; restore the stock caption
// on camp/char-select. Each client is its own process with its own HWND, so multi-boxed
// windows each show their own character.
//
// CRITICAL: this runs from ProcessGameEvents (the game's main loop), which is itself a
// nested window-message context. Calling SetWindowTextA here dispatches WM_SETTEXT inline
// through our subclassed wndproc while wine holds window locks -> re-entrant deadlock (it
// hung the client on the first in-game frame). So we NEVER touch the window synchronously
// here: we compute the desired title (pure, cheap) and PostMessage a private message
// (async, never blocks). The actual SetWindowTextA happens in wndproc_hk when the game
// dispatches that message at the top of its loop - off the ProcessGameEvents stack. Same
// thread throughout, so g_pending_title needs no lock. Opt-in ([Window] CharTitle). ----
void sync_title(HWND h) {
  if (!g_char_title) return;
  std::string want;
  if (Rcp::Game::get_gamestate() == GAMESTATE_INGAME) {
    // pinstLocalPlayer + PlayerBase::Name @ 0xA4 (char[0x40]) - the RoF2 offset proven in-game by
    // keybinds.cpp::self_name(). NOT Entity.Name@0x000 (stale TAKP layout; that read back "q"), and
    // NOT Rcp::Game::get_self() (stale TAKP global 0x7F94CC -> garbage ptr -> crash). See chat_shortcuts.cpp.
    if (char *self = *reinterpret_cast<char **>(0xDD2630)) {
      const char *name = self + 0xA4;
      if (name[0]) want = std::string(name) + " - EverQuest";
    }
  }
  if (want.empty()) want = g_orig_title.empty() ? "EverQuest" : g_orig_title;  // Not in-world -> stock caption.
  if (want == g_posted_title) return;                                          // Only act on a real transition.
  g_posted_title = want;
  g_pending_title = want;
  if (!g_wm_settitle) g_wm_settitle = RegisterWindowMessageA("RofClientPlus_SetWindowTitle");
  PostMessageA(h, g_wm_settitle, 0, 0);  // Non-blocking; applied later in wndproc_hk.
}

void log_health(HWND h, const char *tag) {
  const bool vis = IsWindowVisible(h);
  const bool icon = IsIconic(h);
  HWND fg = GetForegroundWindow();
  const bool is_fg = (fg == h);
  const bool fg_ours = belongs_to_us(fg);
  RECT wr = {};
  GetWindowRect(h, &wr);
  const RECT a = monitor_work(h);
  const DWORD elapsed = g_first_seen ? (GetTickCount() - g_first_seen) : 0;
  logger::logf("[win] %s +%lu.%03lus loop=%lu vis=%d icon=%d fg=%d(ours=%d) rect=(%ld,%ld)-(%ld,%ld) mon=(%ld,%ld)-(%ld,%ld)",
               tag, elapsed / 1000, elapsed % 1000, g_loop, (int)vis, (int)icon, (int)is_fg, (int)fg_ours, wr.left,
               wr.top, wr.right, wr.bottom, a.left, a.top, a.right, a.bottom);
}

// Toolhelp scan for other eqgame.exe instances - the leftover-process suspect.
int scan_siblings(bool log_each) {
  int count = 0;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32 pe = {};
  pe.dwSize = sizeof(pe);
  const DWORD me = GetCurrentProcessId();
  if (Process32First(snap, &pe)) {
    do {
      if (lstrcmpiA(pe.szExeFile, "eqgame.exe") == 0 && pe.th32ProcessID != me) {
        ++count;
        if (log_each)
          logger::logf("[win] sibling eqgame.exe pid=%lu running (multi-box client, or a leftover from a crash)",
                       (unsigned long)pe.th32ProcessID);
      }
    } while (Process32Next(snap, &pe));
  }
  CloseHandle(snap);
  return count;
}

}  // namespace

namespace window_watch {

void install_early() {
  load_settings();

  const char *disp = getenv("DISPLAY");
  logger::logf("[win] install_early: DISPLAY=%s selfheal=%d guard=%d verbose=%d raise=%d dedup-move=%d",
               disp ? disp : "(null)", (int)g_self_heal, (int)g_guard, (int)g_verbose, (int)g_raise, (int)g_dedup);

  // Snapshot the eqclient.ini window keys - the saved rect/mode the client restores.
  IO_ini cini(IO_ini::kClientFilename);
  logger::logf("[win] eqclient.ini WindowedMode=%s Maximized=%s Windowed=%dx%d offset=(%d,%d)",
               cini.getValue<std::string>("Defaults", "WindowedMode").c_str(),
               cini.getValue<std::string>("Defaults", "Maximized").c_str(),
               cini.getValue<int>("VideoMode", "WindowedWidth"), cini.getValue<int>("VideoMode", "WindowedHeight"),
               cini.getValue<int>("Defaults", "WindowedModeXOffset"),
               cini.getValue<int>("Defaults", "WindowedModeYOffset"));

  const int siblings = scan_siblings(true);
  logger::logf("[win] sibling eqgame.exe scan: %d other instance(s)", siblings);

  // Multibox rect dedup (opt-in fallback), creation-time half: runs at attach, BEFORE
  // the game creates its window, so the shifted position is what the window (and the
  // game's own maximize) is born with. See the comment block at install_spawn_hook.
  if (g_dedup) install_spawn_hook();
}

void on_frame() {
  ++g_loop;

  if (!g_hwnd) {
    // The D3D9 device knows the game's real render window exactly; the EnumWindows
    // heuristic (largest captioned/visible top-level owned by us) is a fallback for
    // before the first hooked frame - it never matched under this wine/DXVK setup.
    HWND h = reinterpret_cast<HWND>(directx::get_focus_window());
    const char *via = "d3d";
    if (!h) {
      h = find_main_window();
      via = "enum";
    }
    if (!h) return;
    g_hwnd = h;
    g_first_seen = GetTickCount();
    subclass(h);
    logger::logf("[win] main window via %s", via);
    log_health(h, "found");
    g_last_health = g_first_seen;
    return;
  }

  const bool vis = IsWindowVisible(g_hwnd);
  const bool icon = IsIconic(g_hwnd);
  const bool is_fg = belongs_to_us(GetForegroundWindow());
  const DWORD now = GetTickCount();
  const bool changed = ((int)vis != g_last_vis) || ((int)icon != g_last_icon) || ((int)is_fg != g_last_fg);

  // Dense trace early, then only on state change - enough to see "alive but hidden"
  // (loop climbs, vis=0) vs "never got foreground" (fg=0 persists).
  if (changed || (now - g_first_seen < kHealthDenseMs && now - g_last_health >= kHealthGapMs)) {
    log_health(g_hwnd, changed ? "change" : "trace");
    g_last_health = now;
    g_last_vis = vis;
    g_last_icon = icon;
    g_last_fg = is_fg;
  }

  if (g_self_heal && (now - g_first_seen) < kHealWindowMs) {
    if ((!vis || icon || !is_fg) && (now - g_last_heal) >= kHealMinGapMs) {
      g_last_heal = now;
      heal(g_hwnd, vis, icon, is_fg);
    }
  }

  // Multibox rect dedup (opt-in fallback), startup-window only. First pass is jittered
  // per-process so simultaneously-launched boxes decide in sequence, then re-checks let
  // the set converge (a mover that picked a just-taken monitor re-detects and moves on).
  if (g_dedup && (now - g_first_seen) < kDedupWindowMs) {
    if (!g_dedup_first_delay) g_dedup_first_delay = 2000 + (GetCurrentProcessId() % 5) * 400;
    if ((now - g_first_seen) >= g_dedup_first_delay && (now - g_last_dedup) >= kDedupGapMs) {
      g_last_dedup = now;
      dedup_pass(false);
    }
  }

  sync_title(g_hwnd);  // Put the logged-in character's name in the window title (restored on camp).
}

bool get_self_heal() { return g_self_heal; }
bool get_verbose() { return g_verbose; }
bool get_guard() { return g_guard; }

void set_self_heal(bool on) {
  g_self_heal = on;
  save_settings();
}
void set_verbose(bool on) {
  g_verbose = on;
  save_settings();
}
void set_guard(bool on) {
  g_guard = on;
  rcp_guard::set_enabled(on);
  save_settings();
}
bool get_char_title() { return g_char_title; }
void set_char_title(bool on) {
  g_char_title = on;
  g_posted_title.clear();  // On enable, makes sync_title post the current title next frame. (Off = inert.)
  save_settings();
}
bool get_raise() { return g_raise; }
void set_raise(bool on) {
  g_raise = on;
  save_settings();
}
bool get_dedup() { return g_dedup; }
void set_dedup(bool on) {
  g_dedup = on;
  // The spawn-shift half installs from install_early (pre-window); enabling mid-session
  // only arms the runtime passes + the next launch.
  if (on && !g_real_cwexa) logger::log("[win] dedup: enabled - spawn hook arms on next launch");
  save_settings();
}

// The game window, preferring the D3D device's exact handle over the enum heuristic.
HWND resolve_window() {
  if (HWND h = reinterpret_cast<HWND>(directx::get_focus_window())) return h;
  return find_main_window();
}

void force_heal_now() {
  if (!g_hwnd) g_hwnd = resolve_window();
  if (!g_hwnd) {
    logger::log("[win] force_heal: no window found");
    return;
  }
  heal(g_hwnd, IsWindowVisible(g_hwnd), IsIconic(g_hwnd), belongs_to_us(GetForegroundWindow()));
}

void force_dedup_now() {
  if (!g_hwnd) g_hwnd = resolve_window();
  if (!g_hwnd) {
    logger::log("[win] dedup: no window found");
    return;
  }
  g_dedup_attempts = 0;  // A manual run gets a fresh attempt budget.
  g_dedup_gave_up = false;
  dedup_pass(true);
}

void log_info() {
  if (!g_hwnd) g_hwnd = resolve_window();
  if (g_hwnd)
    log_health(g_hwnd, "info");
  else
    logger::log("[win] info: no main window found");
  const int n = scan_siblings(true);
  logger::logf("[win] info: %d sibling eqgame.exe instance(s); selfheal=%d guard=%d verbose=%d", n, (int)g_self_heal,
               (int)g_guard, (int)g_verbose);
}

}  // namespace window_watch

// ---- /rcpwindow command (self-registers from this module, like the other features). ----
static void print_status() {
  char msg[256];
  std::snprintf(msg, sizeof(msg),
                "rof2ClientPlus window: self-heal=%s | guard-detours=%s | verbose=%s | char-title=%s | "
                "multibox-raise=%s | multibox-move=%s (see rof2ClientPlus.log)",
                window_watch::get_self_heal() ? "ON" : "OFF", rcp_guard::enabled() ? "ON" : "OFF",
                window_watch::get_verbose() ? "ON" : "OFF", window_watch::get_char_title() ? "ON" : "OFF",
                window_watch::get_raise() ? "ON" : "OFF", window_watch::get_dedup() ? "ON" : "OFF");
  Rcp::Game::print_chat(msg);
}

WindowWatch::WindowWatch(RcpService *rcp) : rcp_(rcp) {
  // Settings + diagnostics were already installed from on_attach (install_early); here
  // we only add the command surface.
  rcp->commands_hook->Add(
      "/rcpwindow", {"/rcpwin"},
      "Windowed-startup diagnostics + self-heal. '/rcpwindow on|off' (self-heal), "
      "'/rcpwindow guard on|off', '/rcpwindow verbose on|off', '/rcpwindow title on|off' (character name in "
      "window title), '/rcpwindow raise on|off' (multibox input-routing fix: raise-on-activate), "
      "'/rcpwindow dedup on|off|now' (multibox fallback, off by default: move off a sibling client's "
      "identical window rect), '/rcpwindow heal', '/rcpwindow info'.",
      [](std::vector<std::string> &args) {
        auto onoff = [&](size_t i, bool cur) -> bool {
          if (args.size() > i && (args[i] == "on" || args[i] == "1")) return true;
          if (args.size() > i && (args[i] == "off" || args[i] == "0")) return false;
          return !cur;  // Bare -> toggle.
        };
        if (args.size() >= 2 && args[1] == "guard") {
          window_watch::set_guard(onoff(2, window_watch::get_guard()));
        } else if (args.size() >= 2 && args[1] == "verbose") {
          window_watch::set_verbose(onoff(2, window_watch::get_verbose()));
        } else if (args.size() >= 2 && args[1] == "title") {
          window_watch::set_char_title(onoff(2, window_watch::get_char_title()));
        } else if (args.size() >= 2 && args[1] == "raise") {
          window_watch::set_raise(onoff(2, window_watch::get_raise()));
        } else if (args.size() >= 2 && args[1] == "dedup") {
          if (args.size() > 2 && args[2] == "now") {
            window_watch::force_dedup_now();
            Rcp::Game::print_chat("rof2ClientPlus: ran a dedup pass (see log).");
            return true;
          }
          window_watch::set_dedup(onoff(2, window_watch::get_dedup()));
        } else if (args.size() >= 2 && args[1] == "heal") {
          window_watch::force_heal_now();
          Rcp::Game::print_chat("rof2ClientPlus: forced a window heal (see log).");
          return true;
        } else if (args.size() >= 2 && args[1] == "info") {
          window_watch::log_info();
          Rcp::Game::print_chat("rof2ClientPlus: window info written to rof2ClientPlus.log.");
          return true;
        } else if (args.size() >= 2 && (args[1] == "status")) {
          // fall through to status print
        } else if (args.size() >= 2) {
          window_watch::set_self_heal(onoff(1, window_watch::get_self_heal()));
        } else {
          window_watch::set_self_heal(!window_watch::get_self_heal());  // Bare /rcpwindow toggles self-heal.
        }
        print_status();
        return true;
      });
  logger::log("[win] /rcpwindow registered");
}
