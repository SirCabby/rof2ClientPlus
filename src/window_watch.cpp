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

constexpr char kIni[] = "Window";
constexpr DWORD kHealWindowMs = 8000;  // Only self-heal during the window's first few seconds.
constexpr DWORD kHealMinGapMs = 250;   // Don't fight the client every frame.
constexpr DWORD kHealthDenseMs = 20000;  // Dense per-frame health trace for the first ~20s.
constexpr DWORD kHealthGapMs = 500;

// ---- Runtime state. ----
HWND g_hwnd = nullptr;
WNDPROC g_orig_wndproc = nullptr;
std::string g_orig_title;     // The client's own window caption, captured at subclass; restored when not in-world.
std::string g_pending_title;  // Title the wndproc should apply (see WM_SETTITLE handling); render-thread-only, no lock.
std::string g_posted_title;   // Last title we asked for, so on_frame only re-posts on an actual change.
UINT g_wm_settitle = 0;       // RegisterWindowMessage id: "apply g_pending_title" - handled in wndproc, off the ProcessGameEvents stack.
DWORD g_first_seen = 0;
DWORD g_last_heal = 0;
DWORD g_last_health = 0;
unsigned long g_loop = 0;  // Main-loop iteration count (proxy for "the client is alive").
int g_last_vis = -1, g_last_icon = -1, g_last_fg = -1;

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIni, "SelfHeal")) g_self_heal = ini.getValue<bool>(kIni, "SelfHeal");
  if (ini.exists(kIni, "VerboseLog")) g_verbose = ini.getValue<bool>(kIni, "VerboseLog");
  if (ini.exists(kIni, "GuardDetours")) g_guard = ini.getValue<bool>(kIni, "GuardDetours");
  if (ini.exists(kIni, "CharTitle")) g_char_title = ini.getValue<bool>(kIni, "CharTitle");
  rcp_guard::set_enabled(g_guard);
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIni, "SelfHeal", g_self_heal);
  ini.setValue<bool>(kIni, "VerboseLog", g_verbose);
  ini.setValue<bool>(kIni, "GuardDetours", g_guard);
  ini.setValue<bool>(kIni, "CharTitle", g_char_title);
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
  switch (msg) {
    case WM_ACTIVATEAPP:
      logger::logf("[win] WM_ACTIVATEAPP active=%d", (int)w);
      break;
    case WM_ACTIVATE:
      logger::logf("[win] WM_ACTIVATE %s", LOWORD(w) == WA_INACTIVE ? "INACTIVE" : "ACTIVE");
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
  logger::logf("[win] install_early: DISPLAY=%s selfheal=%d guard=%d verbose=%d", disp ? disp : "(null)",
               (int)g_self_heal, (int)g_guard, (int)g_verbose);

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
                "rof2ClientPlus window: self-heal=%s | guard-detours=%s | verbose=%s | char-title=%s (see rof2ClientPlus.log)",
                window_watch::get_self_heal() ? "ON" : "OFF", rcp_guard::enabled() ? "ON" : "OFF",
                window_watch::get_verbose() ? "ON" : "OFF", window_watch::get_char_title() ? "ON" : "OFF");
  Rcp::Game::print_chat(msg);
}

WindowWatch::WindowWatch(RcpService *rcp) : rcp_(rcp) {
  // Settings + diagnostics were already installed from on_attach (install_early); here
  // we only add the command surface.
  rcp->commands_hook->Add(
      "/rcpwindow", {"/rcpwin"},
      "Windowed-startup diagnostics + self-heal. '/rcpwindow on|off' (self-heal), "
      "'/rcpwindow guard on|off', '/rcpwindow verbose on|off', '/rcpwindow title on|off' (character name in "
      "window title), '/rcpwindow heal', '/rcpwindow info'.",
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
