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
// Multibox input routing ([Window] MultiboxRaise, ON by default): all boxed clients
// share one wineserver session, which routes mouse input by hit-testing the cursor
// against ITS rects in ITS z-order - state KWin's visual stacking and focus never feed
// back into wine, so clicks on one client landed in the other ("clickthrough" / focus
// snapping back). Raise-on-activate mirrors each compositor activation into the
// wineserver z-order, and click-activation is muted while a sibling client runs so a
// stale-routed click can't steal the focus back. [Window] MultiboxMove (/rcpwindow
// dedup, OFF by default) is the older fallback that moves a client off a sibling's
// identical spawn rect; it fought deliberate layouts (maximize kept reverting), so
// it's opt-in only. See "Multibox input routing" in the .cpp.
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
bool get_raise();       // /rcpwindow raise - multibox raise-on-activate input-routing fix (see file comment).
bool get_dedup();       // /rcpwindow dedup - multibox rect-move fallback, opt-in (see file comment).
void set_self_heal(bool on);
void set_verbose(bool on);
void set_guard(bool on);
void set_char_title(bool on);
void set_raise(bool on);
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
