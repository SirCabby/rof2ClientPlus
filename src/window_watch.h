// rof2ClientPlus - windowed-startup diagnostics + opt-in self-heal.
//
// Targets the "after a crash, WindowedMode=TRUE won't start - the window flashes
// open then hides, isn't on the taskbar, and the process lingers" report (Linux /
// wine-staging + DXVK on NVIDIA via XWayland under KWin). The mod is the best-placed
// observer: from inside eqgame.exe it can watch the main window's whole lifecycle.
//
// What it records (to rof2ClientPlus.log):
//   - at attach: DISPLAY, the eqclient.ini window keys (WindowedMode, size, offsets,
//     Maximized), and a scan for any LEFTOVER sibling eqgame.exe still alive - the
//     prime suspect for the windowed-launch failure, since a lingering instance
//     keeps a DXVK/Vulkan context + wineserver window state and blocks the next one.
//   - per frame: a health line (visible / iconic / foreground / window-rect vs
//     monitor) so we can see "alive but hidden" vs "never got foreground".
//   - window messages (activate / show / size / focus / destroy) via a subclass, to
//     tell a self-hide apart from a window-manager unmap.
//
// Opt-in self-heal ([Window] SelfHeal, /rcpwindow on): for the first few seconds of
// the window's life, if it's hidden/minimized/unfocused, force it visible + on-screen
// + foreground. Off by default. NOTE: this only helps if the client reached its main
// loop - a launch that hangs earlier (before our .asi loads) is beyond its reach; for
// that case rely on the crash handler + clearing the leftover process.
//
// Multibox rect dedup ([Window] MultiboxDedup, /rcpwindow dedup, ON by default): boxed
// clients share eqclient.ini, so they all spawn at the SAME wineserver-side rect, and
// wine routes mouse input by hit-testing those rects across every client in the
// session - clicks on one client land in the other ("clickthrough" / focus snapping
// back; KWin never resyncs its placement into wine, so the collision persists even
// when the windows LOOK separated). During its first ~30s the newer client detects the
// identical rect and moves itself to a free monitor. See dedup_pass in the .cpp.
#pragma once

class RcpService;

namespace window_watch {
// Called from dllmain on_attach (before the service exists): loads [Window] settings,
// logs the env/ini snapshot, and scans for leftover sibling eqgame.exe processes.
void install_early();

// Called every frame from ProcessGameEvents_hk: finds + subclasses the main window,
// emits the health trace, and runs the opt-in self-heal. Self-gating and cheap.
void on_frame();

// Live settings for the /rcpwindow command (persisted to rof2ClientPlus.ini [Window]).
bool get_self_heal();
bool get_verbose();
bool get_guard();  // Detour fault-net (rcp_guard) - owned here so it persists.
bool get_char_title();  // /rcpwindow title - show the logged-in character's name in the window title.
bool get_dedup();       // /rcpwindow dedup - multibox rect dedup (see file comment).
void set_self_heal(bool on);
void set_verbose(bool on);
void set_guard(bool on);
void set_char_title(bool on);
void set_dedup(bool on);
void force_heal_now();   // /rcpwindow heal - apply one corrective pass immediately.
void force_dedup_now();  // /rcpwindow dedup now - run one dedup decision pass immediately.
void log_info();         // /rcpwindow info - dump current window state + sibling scan.
}  // namespace window_watch

// Registers the /rcpwindow command. Constructed by RcpService like the other modules;
// the heavy lifting lives in the window_watch namespace (installed earlier, from
// on_attach) so diagnostics run even before the service is built.
class WindowWatch {
 public:
  WindowWatch(RcpService *rcp);

 private:
  RcpService *rcp_ = nullptr;
};
