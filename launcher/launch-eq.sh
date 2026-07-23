#!/usr/bin/env bash
# Launch RoF2 against the local EQEmu server (login target is in eqhost.txt -> 192.168.1.3:5999).
#
# Key detail for this machine (KDE Wayland + NVIDIA): Wine's native Wayland driver
# fails EGL init on the NVIDIA GPU ("failed to create dri2 screen") and the client
# bails during 3D init. Forcing Wine onto XWayland/X11 makes it use NVIDIA's GLX and
# renders correctly -- so we clear WAYLAND_DISPLAY and set DISPLAY below.
#
# 'patchme' skips the live-EverQuest patcher (never let the client patch, or it breaks for emu).

export WINEPREFIX="$HOME/.wine-eq"        # dedicated prefix, won't touch other Wine apps
export WINEDLLOVERRIDES="mscoree=;mshtml=;dinput8=n,b"  # skip Mono/Gecko; load native dinput8.dll (custom-zone injector proxy), builtin fallback
export DISPLAY="${DISPLAY:-:0}"           # XWayland display
unset WAYLAND_DISPLAY                      # <-- force X11/GLX (the fix)

cd "$(dirname "$0")" || exit 1

# Multi-boxing is supported: this script NEVER kills other running clients.
# If a crashed client is truly wedged, run './launch-eq.sh --clean' once with
# nothing you care about running -- it kills ALL eqgame.exe in this prefix
# (wineserver -k included), then launches normally.
if [ "${1:-}" = "--clean" ]; then
    shift
    echo "cleaning ALL eqgame.exe instances + wineserver state..."
    pkill -9 -f 'eqgame\.exe'
    wineserver -k 2>/dev/null
    sleep 2
fi

# KWin/XWayland sometimes drops the game's windowed-mode window right after
# launch (unmapped + parked off-screen -> "flashes open then hides", game keeps
# running invisibly at the login screen). The watcher re-maps / re-centers /
# activates it during the first two minutes. It fixes EVERY eqgame window it
# sees (multi-box safe: re-mapping a healthy window is a no-op, and it leaves
# user-minimized clients alone). Source: rof2ClientPlus repo,
# tools/eq_window_fix.c; installed by `make install`.
if [ -x ./eq-window-fix ]; then
    ./eq-window-fix >> eq-window-fix.log 2>&1 &
    EQWINFIX_PID=$!
fi

# Run the client in its own native window (no Wine virtual desktop). The window
# is then managed by the desktop's own window manager (KDE), so maximize works
# like any other app -- as long as the client is in windowed mode (see note).
wine eqgame.exe patchme "$@" > eq-last-run.log 2>&1
[ -n "${EQWINFIX_PID:-}" ] && kill "$EQWINFIX_PID" 2>/dev/null
