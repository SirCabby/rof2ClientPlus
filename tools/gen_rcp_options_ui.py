#!/usr/bin/env python3
"""Inject the rof2ClientPlus options-window UI into the EQUI_OptionsWindow.xml
override (the custom-UI-skin delivery channel).

WHY THIS SHAPE - a day of world-entry crashes taught us how this client loads UI:

1. Adding our window as a SEPARATE INCLUDED FILE crashes the client. Every
   delivery of a standalone EQUI_RcpOptions.xml (nested <Composite> include,
   or an <Include> merged into EQUI.xml at the world-entry LoadSidl) died with
   an illegal-instruction jump into .rsrc during "Parsing UI XML" (dbg.txt
   fatal c000001d, UIErrors.txt empty) once the window grew past ~a dozen
   controls - it worked at 11 controls, so it smells like a latent client bug
   in the include-merge path whose corruption scales with parse churn. The
   same file PARSED FINE standalone, so the content is valid.
2. Calling CSidlManagerBase::LoadSidl(0x870C60) at runtime is DESTRUCTIVE, not
   additive: it clears the manager (0x86d7a0) and rebuilds the whole template
   set from the given file as composite root - doing it in-game orphaned every
   live window's templates (visually broke the UI) and is not a usable route.
3. What IS proven, both by us and by every community custom UI skin on this
   client: shipping a MODIFIED COPY OF A STOCK FILE in the skin folder. Our
   uifiles/rcp/EQUI_OptionsWindow.xml override loaded through every crash run
   without complaint. So the RcpOptions window rides in THAT file: its control
   defs + Screen are appended between marker comments, the client parses them
   as part of its normal per-file skin fallback, and /rcpoptions merely
   instantiates the already-registered template.

LAYOUT: a TABBED window. The "tabs" are a strip of checkbox buttons + per-tab
control groups that the C++ shows/hides (a real SIDL TabBox/Page renders but
does not route mouse input when the window is instantiated by us at runtime,
so only proven primitives are used: Button/Label/Slider as direct Screen
children). Tab contents OVERLAP in the same region; one group visible at a
time. The Colors tab is Zeal-style: one button per nameplate color role, its
text tinted the live color at runtime (CRNormal@0x12c); clicking opens the
client's stock color picker (CColorPickerWnd::Open 0x659AF0).

ORDER CONTRACT: the ROLES list below must match the NpColorRole enum order in
src/nameplate.cpp, and control ScreenIDs must match src/rcp_options_ui.cpp.

Also: ONE ELEMENT PER LINE (no stock file ever inlines
`<Location><X>..</X>..`), no `--` inside XML comments, plain-ASCII Text.

This script REQUIRES uifiles/rcp/EQUI_OptionsWindow.xml to exist (generate it
first with tools/gen_option_overrides.py) and is idempotent: it replaces any
previous marker block. Run order: gen_option_overrides.py, then this.

Usage:  python3 tools/gen_rcp_options_ui.py
"""

import os

# ---- colors ----
WHITE = (255, 255, 255)
YELLOW = (255, 255, 0)

# ---- window + layout ----
WINDOW_CX = 356  # Wide enough for the 5-tab strip at the proven 64 px tab width.
WINDOW_CY = 356  # Tall enough for the Nameplate tab (billboard + bar toggles + 7 toggles + 2 sliders).
TAB_Y = 6            # tab-strip row
TAB_W, TAB_H = 64, 20
CONTENT_Y = 34       # first content row under the tab strip
COL_X = 12           # content column x
SLIDER_W = 190
VAL_X = 208          # value-label x (right of sliders)

# Nameplate color roles: MUST mirror the NpColorRole enum order in
# src/nameplate.cpp (index == role). Names are what players call the bands.
# (No "other guild": players with no special state keep the client's default color.)
ROLES = [
    "Con: even", "Con: yellow", "Con: red", "Con: green", "Con: light blue", "Con: blue",
    "Target", "PvP", "AFK", "Linkdead", "LFG", "Group", "Raid", "Roleplay",
    "My guild", "Corpse", "GM", "Player",
]


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def loc_size(x, y, cx, cy):
    return [
        "    <Location>",
        f"      <X>{x}</X>",
        f"      <Y>{y}</Y>",
        "    </Location>",
        "    <Size>",
        f"      <CX>{cx}</CX>",
        f"      <CY>{cy}</CY>",
        "    </Size>",
    ]


def color(tag, rgb):
    r, g, b = rgb
    return [
        f"    <{tag}>",
        f"      <R>{r}</R>",
        f"      <G>{g}</G>",
        f"      <B>{b}</B>",
        f"    </{tag}>",
    ]


def button(name, x, y, cx, cy, text, tooltip, rgb=WHITE):
    out = [f'  <Button item="{name}">', f"    <ScreenID>{name}</ScreenID>", "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <Style_Checkbox>true</Style_Checkbox>", f"    <TooltipReference>{esc(tooltip)}</TooltipReference>",
            f"    <Text>{esc(text)}</Text>"]
    out += color("TextColor", rgb)
    out += ["    <ButtonDrawTemplate>", "      <Normal>A_BtnNormal</Normal>", "      <Pressed>A_BtnPressed</Pressed>",
            "      <Flyby>A_BtnFlyby</Flyby>", "      <Disabled>A_BtnDisabled</Disabled>",
            "      <PressedFlyby>A_BtnPressedFlyby</PressedFlyby>", "    </ButtonDrawTemplate>", "  </Button>"]
    return out


def label(name, x, y, cx, cy, text, rgb=WHITE):
    out = [f'  <Label item="{name}">', f"    <ScreenID>{name}</ScreenID>", "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += [f"    <Text>{esc(text)}</Text>"]
    out += color("TextColor", rgb)
    out += ["    <NoWrap>true</NoWrap>", "  </Label>"]
    return out


def slider(name, x, y, cx, cy):
    out = [f'  <Slider item="{name}">', f"    <ScreenID>{name}</ScreenID>", "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <SliderArt>SDT_DefSlider</SliderArt>", "  </Slider>"]
    return out


def build_controls():
    """Returns the control list; the order is also the <Pieces> order."""
    c = []

    # ---- Tab strip (checkbox buttons; C++ enforces radio behavior) ----
    tabs = [("Rcp_TabMouse", "Mouse", "Mouse-look settings"),
            ("Rcp_TabCamera", "Camera", "Chase-camera settings"),
            ("Rcp_TabNameplate", "Nameplate", "Nameplate toggles"),
            ("Rcp_TabColors", "Colors", "Nameplate colors"),
            ("Rcp_TabDisplay", "Display", "Display and world settings")]
    for i, (name, text, tip) in enumerate(tabs):
        c.append((name, button, (name, COL_X + i * (TAB_W + 3), TAB_Y, TAB_W, TAB_H, text, tip)))

    # ---- Tab 0: Mouse ----
    y = CONTENT_Y
    c.append(("Rcp_Enabled", button, ("Rcp_Enabled", COL_X, y, 250, 20, "Enable mouse-look sensitivity",
                                      "Enable rof2ClientPlus mouse-look sensitivity")))
    y += 26
    c.append(("Rcp_SensXLabel", label, ("Rcp_SensXLabel", COL_X, y, 170, 14, "Sensitivity X (horizontal)")))
    c.append(("Rcp_SensX", slider, ("Rcp_SensX", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_SensXValue", label, ("Rcp_SensXValue", VAL_X, y + 16, 58, 16, "1.0", YELLOW)))
    y += 42
    c.append(("Rcp_SensYLabel", label, ("Rcp_SensYLabel", COL_X, y, 170, 14, "Sensitivity Y (vertical)")))
    c.append(("Rcp_SensY", slider, ("Rcp_SensY", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_SensYValue", label, ("Rcp_SensYValue", VAL_X, y + 16, 58, 16, "1.0", YELLOW)))
    y += 42
    c.append(("Rcp_SmoothLabel", label, ("Rcp_SmoothLabel", COL_X, y, 170, 14, "Smoothing")))
    c.append(("Rcp_Smooth", slider, ("Rcp_Smooth", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_SmoothValue", label, ("Rcp_SmoothValue", VAL_X, y + 16, 58, 16, "0.00", YELLOW)))
    y += 42
    c.append(("Rcp_LockMouse", button, ("Rcp_LockMouse", COL_X, y, 250, 20, "Lock mouse to window",
                                        "Confine the mouse cursor to the game window (released when you alt-tab away)")))

    # ---- Tab 1: Camera (chase) ----
    y = CONTENT_Y
    c.append(("Rcp_ChaseEnabled", button, ("Rcp_ChaseEnabled", COL_X, y, 250, 20, "Enable chase camera",
                                           "Adjust the mouse-wheel third-person chase camera (/rcpchase)")))
    y += 22
    c.append(("Rcp_ChaseCollision", button, ("Rcp_ChaseCollision", COL_X, y, 250, 20, "Wall collision pull-in",
                                             "Pull the camera in when world geometry blocks the view")))
    y += 26
    c.append(("Rcp_ChaseDistLabel", label, ("Rcp_ChaseDistLabel", COL_X, y, 200, 14, "Max zoom out (0 = native)")))
    c.append(("Rcp_ChaseDist", slider, ("Rcp_ChaseDist", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_ChaseDistValue", label, ("Rcp_ChaseDistValue", VAL_X, y + 16, 58, 16, "native", YELLOW)))

    # ---- Tab 2: Nameplate (custom billboard + color toggles + sliders) ----
    # (Name generation is always active - not an option.)
    # Custom-font billboard master toggle (drives font_overlay, hides the default names).
    c.append(("Rcp_NpBillboard", button, ("Rcp_NpBillboard", COL_X, CONTENT_Y, 250, 20,
                                          "Custom nameplates (3D font + bars)",
                                          "Draw custom-font 3D nameplates and hide the default names (/rcpfont)")))
    # Per-bar toggles on one row (HP for all; mana/stamina self only).
    bar_y = CONTENT_Y + 22
    c.append(("Rcp_NpHpBar", button, ("Rcp_NpHpBar", COL_X, bar_y, 84, 20, "HP bar", "Show the health bar")))
    c.append(("Rcp_NpManaBar", button, ("Rcp_NpManaBar", COL_X + 88, bar_y, 84, 20, "Mana bar",
                                        "Show your mana bar (self only)")))
    c.append(("Rcp_NpStamBar", button, ("Rcp_NpStamBar", COL_X + 176, bar_y, 84, 20, "Stam bar",
                                        "Show your stamina bar (self only)")))
    # Color / target toggles.
    np = [("Rcp_NpConColors", "Con colors (NPCs)", "Tint NPC nameplates by con level"),
          ("Rcp_NpStateColors", "State colors (players)", "Tint players by guild/AFK/LFG/LD/roleplay/PVP; corpses too"),
          ("Rcp_NpTargetColor", "Target color", "Highlight the current target's nameplate"),
          ("Rcp_NpTargetBlink", "Target blink", "Pulse the target's nameplate (any coloring mode, PCs and NPCs)"),
          ("Rcp_NpTargetMarker", "Target name markers", "Wrap the target's name in brackets"),
          ("Rcp_NpTargetHealth", "Target health %", "Append the target's HP percent to its nameplate"),
          ("Rcp_NpHideSelf", "Hide own nameplate", "Blank your own nameplate (unless it is your target)")]
    np_y = CONTENT_Y + 44
    for i, (name, text, tip) in enumerate(np):
        c.append((name, button, (name, COL_X, np_y + i * 22, 250, 20, text, tip)))
    y = np_y + len(np) * 22 + 4
    c.append(("Rcp_BlinkSpeedLabel", label, ("Rcp_BlinkSpeedLabel", COL_X, y, 170, 14, "Blink speed (cycle time)")))
    c.append(("Rcp_BlinkSpeed", slider, ("Rcp_BlinkSpeed", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_BlinkSpeedValue", label, ("Rcp_BlinkSpeedValue", VAL_X, y + 16, 58, 16, "1.20s", YELLOW)))
    y += 40
    c.append(("Rcp_NpDistLabel", label, ("Rcp_NpDistLabel", COL_X, y, 200, 14, "Nameplate draw distance")))
    c.append(("Rcp_NpDist", slider, ("Rcp_NpDist", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_NpDistValue", label, ("Rcp_NpDistValue", VAL_X, y + 16, 58, 16, "5000", YELLOW)))

    # ---- Tab 3: Nameplate colors (Zeal-style role buttons; runtime tints the
    #      text via CRNormal and opens the stock color picker on click) ----
    for i, role in enumerate(ROLES):
        col, row = (0, i) if i < 9 else (1, i - 9)
        c.append((f"Rcp_Role{i}", button, (f"Rcp_Role{i}", COL_X + col * 132, CONTENT_Y + row * 20, 128, 18, role,
                                           f"Edit the '{role}' nameplate color")))

    # ---- Tab 4: Display ----
    c.append(("Rcp_NoFog", button, ("Rcp_NoFog", COL_X, CONTENT_Y, 250, 20, "Remove distance fog",
                                    "Remove the client's distance fog haze in every zone, day and night (/rcpfog)")))
    return c


CONTROLS = build_controls()

MARK_BEGIN = "  <!-- RCP-OPTIONS-BEGIN (generated by tools/gen_rcp_options_ui.py; do not hand-edit) -->"
MARK_END = "  <!-- RCP-OPTIONS-END -->"


def gen_rcp_block():
    """The marker-delimited block: all control defs, then the RcpOptions Screen."""
    lines = [MARK_BEGIN,
             "  <!-- The rof2ClientPlus options window rides inside this stock-file override",
             "       (the custom-UI-skin channel) because every standalone-file delivery",
             "       crashed the client's UI parse; see tools/gen_rcp_options_ui.py. The",
             "       window is TABBED: the Rcp_Tab* buttons + per-tab control groups whose",
             "       visibility the mod toggles (a real TabBox does not route input when the",
             "       window is created at runtime). Groups overlap in the same region. -->", ""]
    for _name, fn, args in CONTROLS:
        lines += fn(*args)
        lines.append("")
    lines += ['  <Screen item="RcpOptions">', "    <ScreenID>RcpOptions</ScreenID>",
              "    <RelativePosition>false</RelativePosition>", "    <Location>", "      <X>10</X>", "      <Y>10</Y>",
              "    </Location>", "    <Size>", f"      <CX>{WINDOW_CX}</CX>", f"      <CY>{WINDOW_CY}</CY>", "    </Size>",
              "    <Text>rof2ClientPlus Options</Text>", "    <Style_VScroll>false</Style_VScroll>",
              "    <Style_HScroll>false</Style_HScroll>", "    <Style_Transparent>false</Style_Transparent>",
              "    <TooltipReference />", "    <DrawTemplate>WDT_Def</DrawTemplate>",
              "    <Style_Titlebar>true</Style_Titlebar>", "    <Style_Closebox>true</Style_Closebox>",
              "    <Style_Minimizebox>true</Style_Minimizebox>", "    <Style_Border>true</Style_Border>",
              "    <Style_Sizable>false</Style_Sizable>"]
    for name, _fn, _args in CONTROLS:
        lines.append(f"    <Pieces>{name}</Pieces>")
    lines += ["  </Screen>", MARK_END]
    return "\n".join(lines)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "uifiles", "rcp")
    host = os.path.join(out_dir, "EQUI_OptionsWindow.xml")
    if not os.path.exists(host):
        raise SystemExit("uifiles/rcp/EQUI_OptionsWindow.xml missing; run tools/gen_option_overrides.py first")
    text = open(host, encoding="ascii").read()

    # Idempotency: strip any previous injected block.
    if MARK_BEGIN in text:
        pre, rest = text.split(MARK_BEGIN, 1)
        _, post = rest.split(MARK_END, 1)
        text = pre + post.lstrip("\n")

    # Insert the block just before the closing </XML> tag.
    close = text.rfind("</XML>")
    if close < 0:
        raise SystemExit("no </XML> in EQUI_OptionsWindow.xml override?")
    text = text[:close].rstrip("\n") + "\n\n" + gen_rcp_block() + "\n\n" + text[close:]
    with open(host, "w", encoding="ascii") as f:
        f.write(text)

    # Earlier deliveries shipped standalone files; make sure they can't linger.
    for stale in ("EQUI_RcpOptions.xml", "EQUI_Tab_Cam.xml"):
        p = os.path.join(out_dir, stale)
        if os.path.exists(p):
            os.remove(p)
            print(f"removed stale {stale}")
    print(f"injected {len(CONTROLS)} controls + Screen into EQUI_OptionsWindow.xml override (tabbed)")


if __name__ == "__main__":
    main()
