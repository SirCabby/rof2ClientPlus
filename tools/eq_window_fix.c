/* eq-window-fix - keep RoF2 windowed-mode windows visible under KWin/XWayland.
 *
 * The bug (KDE Wayland + NVIDIA + XWayland, verified live 2026-07-07): in
 * windowed mode, Wine maps the game's X11 window briefly, then the window ends
 * up UNMAPPED (and sometimes parked outside the visible area) while the game
 * keeps running normally at the login screen. KWin drops unmapped windows from
 * management, so there is no taskbar entry and no alt-tab: the classic
 * "flashes open then hides, process remains" failure. One XMapRaised makes
 * KWin adopt and manage the window again (verified: it then appears in KWin's
 * window list, activates, and behaves like any native window).
 *
 * This is a tiny host-side X11 watcher that runs alongside a launch for the
 * first couple of minutes (the unmap is a launch-time race):
 *   - scans for ALL of the game's top-level windows (class "eqgame", not
 *     override-redirect, reasonably sized - skips Wine's 1x1 IME helpers).
 *     MULTI-BOX SAFE: every client's window is checked; healthy windows are
 *     untouched (re-map of a mapped window is a no-op) and windows the user
 *     minimized are left alone (WM_STATE IconicState guard).
 *   - whenever one is UNMAPPED (and not just minimized): re-map + activate it
 *   - whenever one sits fully outside the X screen span: re-center it
 *   - exits when no game window remains (all clients closed) or at --timeout
 *
 * Build (host gcc, NOT the mingw cross-compiler):
 *   gcc -O2 -o eq-window-fix eq_window_fix.c -lX11     (or just `make`)
 * Run: started per-launch by launch-eq.sh; concurrent instances are harmless
 * (all operations are idempotent).
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int xerr_ignore(Display *d, XErrorEvent *e) {
  (void)d;
  (void)e;
  return 0; /* windows vanish mid-query all the time; never die on BadWindow */
}

static void logline(const char *fmt, ...) {
  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);
  fprintf(stdout, "[%02d:%02d:%02d %d] ", tm.tm_hour, tm.tm_min, tm.tm_sec, (int)getpid());
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  fputc('\n', stdout);
  fflush(stdout);
}

static int class_is_eqgame(Display *dpy, Window w) {
  XClassHint cls = {0};
  int match = 0;
  if (XGetClassHint(dpy, w, &cls)) {
    match = (cls.res_class && strstr(cls.res_class, "eqgame")) ||
            (cls.res_name && strstr(cls.res_name, "eqgame"));
    if (cls.res_name) XFree(cls.res_name);
    if (cls.res_class) XFree(cls.res_class);
  }
  return match;
}

/* WM_STATE first field: 0=Withdrawn 1=Normal 3=Iconic. -1 if absent. */
static long wm_state(Display *dpy, Window w) {
  Atom a = XInternAtom(dpy, "WM_STATE", False), type;
  int fmt;
  unsigned long n, left;
  unsigned char *data = NULL;
  long state = -1;
  if (XGetWindowProperty(dpy, w, a, 0, 2, False, a, &type, &fmt, &n, &left, &data) == Success &&
      data) {
    if (n >= 1) state = ((long *)data)[0];
    XFree(data);
  }
  return state;
}

static void activate(Display *dpy, Window w) {
  XEvent ev = {0};
  ev.xclient.type = ClientMessage;
  ev.xclient.window = w;
  ev.xclient.message_type = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 1; /* source: application */
  ev.xclient.data.l[1] = CurrentTime;
  XSendEvent(dpy, DefaultRootWindow(dpy), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

/* One scan pass: check/fix EVERY game window. Returns how many were found. */
static int fix_game_windows(Display *dpy, int sw, int sh, int *restores) {
  Window root = DefaultRootWindow(dpy), parent, *kids = NULL;
  unsigned int nkids = 0;
  int found = 0;
  if (!XQueryTree(dpy, root, &root, &parent, &kids, &nkids)) return 0;

  for (unsigned int i = 0; i < nkids; i++) {
    XWindowAttributes at;
    if (!XGetWindowAttributes(dpy, kids[i], &at)) continue;
    /* Real client windows only - skip Wine's 1x1 override-redirect IME helpers. */
    if (at.override_redirect || at.width < 200 || at.height < 200) continue;
    if (!class_is_eqgame(dpy, kids[i])) continue;
    found++;

    int fixed = 0;

    /* Withdrawn/unmanaged (the bug) - but leave a user-minimized client alone. */
    if (at.map_state != IsViewable && wm_state(dpy, kids[i]) != 3 /* IconicState */) {
      XMapRaised(dpy, kids[i]);
      logline("window 0x%lx was UNMAPPED - re-mapped it (restore #%d)", kids[i], ++*restores);
      fixed = 1;
    }

    /* Parked fully outside the X screen span - recenter. (The span covers all
     * monitors; a window merely on another monitor is left where it is.) */
    if (at.x >= sw || at.y >= sh || at.x + at.width <= 0 || at.y + at.height <= 0) {
      int nx = (sw - at.width) / 2, ny = (sh - at.height) / 2;
      if (nx < 0) nx = 0;
      if (ny < 0) ny = 0;
      XMoveWindow(dpy, kids[i], nx, ny);
      logline("window 0x%lx was off-screen at %d,%d - moved to %d,%d", kids[i], at.x, at.y, nx, ny);
      fixed = 1;
    }

    if (fixed) {
      activate(dpy, kids[i]);
      XFlush(dpy);
    }
  }
  if (kids) XFree(kids);
  return found;
}

int main(int argc, char **argv) {
  int timeout = 120; /* the unmap is a launch-time race; no need to run forever */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--timeout") && i + 1 < argc) timeout = atoi(argv[++i]);
  }

  Display *dpy = XOpenDisplay(NULL);
  if (!dpy) {
    logline("eq-window-fix: cannot open X display - nothing to do");
    return 1;
  }
  XSetErrorHandler(xerr_ignore);
  const int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  const int sh = DisplayHeight(dpy, DefaultScreen(dpy));
  logline("eq-window-fix: watching for EverQuest windows (X span %dx%d, timeout %ds)", sw, sh,
          timeout);

  time_t start = time(NULL), last_seen = 0;
  int restores = 0, seen = 0, last_found = -1;

  while (time(NULL) - start < timeout) {
    int found = fix_game_windows(dpy, sw, sh, &restores);
    if (found != last_found) {
      logline("%d game window%s in view", found, found == 1 ? "" : "s");
      last_found = found;
    }
    if (found > 0) {
      seen = 1;
      last_seen = time(NULL);
    } else if (seen && last_seen && time(NULL) - last_seen > 15) {
      logline("no game windows left for 15s - exiting");
      break;
    }
    usleep(400 * 1000);
  }

  logline("done (%d restore%s)", restores, restores == 1 ? "" : "s");
  XCloseDisplay(dpy);
  return 0;
}
