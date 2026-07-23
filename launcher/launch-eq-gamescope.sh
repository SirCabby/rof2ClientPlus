#!/usr/bin/env bash
# Launch RoF2 inside a gamescope window — the game gets its own micro-compositor
# whose output is a NORMAL desktop window (move / resize / maximize under KDE like
# any other app). This is NOT a Wine virtual desktop (no explorer.exe shell); it
# sidesteps the KWin/XWayland interaction that hides the client's own window in
# windowed mode.
#
# Verified 2026-07-07: window opens and stays, login + server select reachable.
# (In the earlier diagnostic run the game "closed on its own" at server select only
# because the TEST script auto-killed everything after its 70-second watch window —
# this launcher never kills the game.)
#
# Super+F toggles the gamescope window fullscreen. 'patchme' skips the live-EQ
# patcher (required for emu).

export WINEPREFIX="$HOME/.wine-eq"          # dedicated prefix, same as launch-eq.sh
export WINEDLLOVERRIDES="mscoree=;mshtml="  # skip Mono/Gecko prompts

cd "$(dirname "$0")" || exit 1

# Multi-boxing is supported: this script NEVER kills other running clients.
# If a crashed client is truly wedged, run with --clean once (nothing you care
# about running) to kill ALL eqgame.exe in this prefix + reset wineserver.
if [ "${1:-}" = "--clean" ]; then
    shift
    echo "cleaning ALL eqgame.exe instances + wineserver state..."
    pkill -9 -f 'eqgame\.exe'
    wineserver -k 2>/dev/null
    sleep 2
fi

# -W/-H = size of the desktop window gamescope creates. The game inside can stay
# WindowedMode=TRUE (this is what was tested), or FALSE to let it think it is
# fullscreen while gamescope still presents it as a window. Add --force-grab-cursor
# before the '--' if mouselook ever escapes the window.
exec gamescope -W 2560 -H 1440 -- wine eqgame.exe patchme "$@" > eq-last-run.log 2>&1
